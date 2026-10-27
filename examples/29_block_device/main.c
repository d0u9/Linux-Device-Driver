// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/bio.h>
#include <linux/blk-mq.h>
#include <linux/blkdev.h>
#include <linux/log2.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

#include "main.h"

static unsigned long nsectors = 32768;
module_param(nsectors, ulong, 0444);
MODULE_PARM_DESC(nsectors, "disk size in blocks of hardsect_size (default 32768)");

static int hardsect_size = 512;
module_param(hardsect_size, int, 0444);
MODULE_PARM_DESC(hardsect_size, "logical block size: 512 up to the page size (default 512)");

enum {
	RM_SIMPLE  = 0,	/* blk-mq, a request walked segment by segment */
	RM_FULL    = 1,	/* blk-mq, a request walked bio by bio */
	RM_NOQUEUE = 2,	/* bio-based, ->submit_bio(), no request queue */
};
static int request_mode = RM_SIMPLE;
module_param(request_mode, int, 0444);
MODULE_PARM_DESC(request_mode, "0: blk-mq, by segment (default), 1: blk-mq, by bio, 2: bio-based");

static struct memdisk *memdisk;

/*
 * Copy one segment between the disk and memory. The block layer counts
 * sectors in 512 bytes, whatever the disk's block size. A bio_vec is a
 * page, an offset and a length; memcpy_{from,to}_bvec() map the page for
 * the copy.
 */
static blk_status_t memdisk_transfer(struct memdisk *dev, sector_t sector,
				     struct bio_vec *bvec, bool write)
{
	u64 offset = (u64)sector << SECTOR_SHIFT;

	if (offset + bvec->bv_len > dev->size)
		return BLK_STS_IOERR;

	if (write)
		memcpy_from_bvec(dev->data + offset, bvec);
	else
		memcpy_to_bvec(bvec, dev->data + offset);
	return BLK_STS_OK;
}

/* Transfer every segment of one bio, which covers consecutive sectors. */
static blk_status_t memdisk_xfer_bio(struct memdisk *dev, struct bio *bio)
{
	sector_t sector = bio->bi_iter.bi_sector;
	bool write = bio_data_dir(bio) == WRITE;
	blk_status_t status;
	struct bvec_iter iter;
	struct bio_vec bvec;

	bio_for_each_segment(bvec, bio, iter) {
		status = memdisk_transfer(dev, sector, &bvec, write);
		if (status)
			return status;
		sector += bvec.bv_len >> SECTOR_SHIFT;
	}
	return BLK_STS_OK;
}

static bool memdisk_rw(enum req_op op)
{
	return op == REQ_OP_READ || op == REQ_OP_WRITE;
}

/*
 * blk-mq: the block layer has merged adjacent bios into a request and may
 * have reordered requests with its scheduler. A request covers
 * consecutive sectors; it is a list of bios, each a list of segments.
 *
 * Simple: walk the request's segments, across all its bios, in one loop.
 */
static blk_status_t memdisk_queue_rq_simple(struct blk_mq_hw_ctx *hctx,
					    const struct blk_mq_queue_data *bd)
{
	struct request *rq = bd->rq;
	struct memdisk *dev = rq->q->queuedata;
	sector_t sector = blk_rq_pos(rq);
	bool write = rq_data_dir(rq) == WRITE;
	blk_status_t status = BLK_STS_NOTSUPP;
	struct req_iterator iter;
	struct bio_vec bvec;

	blk_mq_start_request(rq);

	if (memdisk_rw(req_op(rq))) {
		rq_for_each_segment(bvec, rq, iter) {
			status = memdisk_transfer(dev, sector, &bvec, write);
			if (status)
				break;
			sector += bvec.bv_len >> SECTOR_SHIFT;
		}
	}

	/*
	 * The request is completed here, so return OK whatever its status:
	 * an error return would make blk-mq complete it a second time.
	 */
	blk_mq_end_request(rq, status);
	return BLK_STS_OK;
}

/*
 * Full: walk the same request bio by bio, then each bio segment by
 * segment. It moves the same data as the simple version; it only spells
 * out the two levels that rq_for_each_segment() hides. In LDD3 the two
 * differed: the simple request function did only the request's current
 * chunk per call and completed it in pieces, which blk-mq no longer allows.
 */
static blk_status_t memdisk_queue_rq_full(struct blk_mq_hw_ctx *hctx,
					  const struct blk_mq_queue_data *bd)
{
	struct request *rq = bd->rq;
	struct memdisk *dev = rq->q->queuedata;
	blk_status_t status = BLK_STS_NOTSUPP;
	struct bio *bio;

	blk_mq_start_request(rq);

	if (memdisk_rw(req_op(rq))) {
		__rq_for_each_bio(bio, rq) {
			status = memdisk_xfer_bio(dev, bio);
			if (status)
				break;
		}
	}

	blk_mq_end_request(rq, status);
	return BLK_STS_OK;
}

static const struct blk_mq_ops memdisk_mq_ops_simple = {
	.queue_rq = memdisk_queue_rq_simple,
};

static const struct blk_mq_ops memdisk_mq_ops_full = {
	.queue_rq = memdisk_queue_rq_full,
};

/*
 * bio-based: no request queue, no merging, no scheduler; each bio comes
 * here as it is submitted. Stacking drivers (dm, md) and RAM disks (brd,
 * zram) work this way.
 */
static void memdisk_submit_bio(struct bio *bio)
{
	struct memdisk *dev = bio->bi_bdev->bd_disk->private_data;

	if (memdisk_rw(bio_op(bio)))
		bio->bi_status = memdisk_xfer_bio(dev, bio);
	else
		bio->bi_status = BLK_STS_NOTSUPP;
	bio_endio(bio);
}

static const struct block_device_operations memdisk_fops = {
	.owner = THIS_MODULE,
};

static const struct block_device_operations memdisk_bio_fops = {
	.owner      = THIS_MODULE,
	.submit_bio = memdisk_submit_bio,
};

static int memdisk_create_disk(struct memdisk *dev)
{
	struct queue_limits lim = {
		.logical_block_size = hardsect_size,
	};
	struct gendisk *gd;
	int err;

	if (request_mode == RM_NOQUEUE) {
		gd = blk_alloc_disk(&lim, NUMA_NO_NODE);
	} else {
		/* one hardware queue, up to 128 requests in flight */
		err = blk_mq_alloc_sq_tag_set(&dev->tag_set,
					      request_mode == RM_FULL ?
					      &memdisk_mq_ops_full :
					      &memdisk_mq_ops_simple,
					      128, 0);
		if (err)
			return err;
		gd = blk_mq_alloc_disk(&dev->tag_set, &lim, dev);
	}
	if (IS_ERR(gd)) {
		err = PTR_ERR(gd);
		goto err_tags;
	}

	gd->major = dev->major;
	gd->first_minor = 0;
	gd->minors = MEMDISK_MINORS;
	gd->fops = request_mode == RM_NOQUEUE ? &memdisk_bio_fops : &memdisk_fops;
	gd->private_data = dev;
	strscpy(gd->disk_name, MODULE_NAME);
	set_capacity(gd, dev->size >> SECTOR_SHIFT);	/* in 512-byte sectors */

	/* the disk goes live: partitions are scanned, /dev/memdisk appears */
	err = add_disk(gd);
	if (err) {
		put_disk(gd);
		goto err_tags;
	}

	dev->gd = gd;
	return 0;

err_tags:
	if (request_mode != RM_NOQUEUE)
		blk_mq_free_tag_set(&dev->tag_set);
	return err;
}

static void memdisk_destroy_disk(struct memdisk *dev)
{
	del_gendisk(dev->gd);
	put_disk(dev->gd);
	if (request_mode != RM_NOQUEUE)
		blk_mq_free_tag_set(&dev->tag_set);
}

static int __init m_init(void)
{
	int err;

	if (!is_power_of_2(hardsect_size) || hardsect_size < SECTOR_SIZE ||
	    hardsect_size > PAGE_SIZE || !nsectors ||
	    request_mode < RM_SIMPLE || request_mode > RM_NOQUEUE)
		return -EINVAL;

	memdisk = kzalloc_obj(*memdisk);
	if (!memdisk)
		return -ENOMEM;

	memdisk->size = (u64)nsectors * hardsect_size;
	memdisk->data = vzalloc(memdisk->size);
	if (!memdisk->data) {
		err = -ENOMEM;
		goto err_free;
	}

	/*
	 * A major of our own, shown in /proc/devices. Without one, leaving
	 * gd->major 0, add_disk() takes numbers from the shared dynamic
	 * block major instead.
	 */
	memdisk->major = register_blkdev(0, MODULE_NAME);
	if (memdisk->major < 0) {
		err = memdisk->major;
		goto err_vfree;
	}

	err = memdisk_create_disk(memdisk);
	if (err)
		goto err_major;

	return 0;

err_major:
	unregister_blkdev(memdisk->major, MODULE_NAME);
err_vfree:
	vfree(memdisk->data);
err_free:
	kfree(memdisk);
	return err;
}

static void __exit m_exit(void)
{
	memdisk_destroy_disk(memdisk);
	unregister_blkdev(memdisk->major, MODULE_NAME);
	vfree(memdisk->data);
	kfree(memdisk);
}

module_init(m_init);
module_exit(m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("A RAM disk with blk-mq or bio-based I/O");
