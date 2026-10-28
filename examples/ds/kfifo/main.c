// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kfifo.h>

/*
 * A ring of 8 chars, buffer included, ready to use: no allocation and
 * nothing to free. The size must be a power of 2. Kept here so
 * dump_bytes() can inspect the typed buffer.
 *
 * One reader and one writer can run without a lock, provided there is
 * no concurrent reset. Multiple readers or writers need serialization.
 * This example has only one thread accessing the fifos.
 */
static DEFINE_KFIFO(bytes, char, 8);

/*
 * Draw the ring with the slots in use and the free ones as '.'. @in and
 * @out only ever grow; the slot is the counter masked with size - 1, so
 * the length is in - out even after the counters wrap past UINT_MAX.
 * That is why the size must be a power of 2.
 */
static void dump_bytes(const char *title)
{
	unsigned int in = bytes.kfifo.in, out = bytes.kfifo.out;
	unsigned int mask = bytes.kfifo.mask, i;
	char ring[16];

	for (i = 0; i <= mask; i++)
		ring[i] = ((i - out) & mask) < in - out ? bytes.buf[i] : '.';
	ring[i] = '\0';
	pr_info("%-22s [%s] out=%u in=%u len=%u avail=%u\n", title, ring,
		out, in, kfifo_len(&bytes), kfifo_avail(&bytes));
}

static int __init kfifo_demo(void)
{
	/* a ring of 4 ints: the element type is part of the fifo's type */
	DEFINE_KFIFO(ints, int, 4);

	/*
	 * A record fifo: 32 bytes holding variable-length records, each with a
	 * 1-byte length in front. Each kfifo_out() consumes one record, but
	 * copies only as much as fits in the destination.
	 */
	STRUCT_KFIFO_REC_1(32) recs;

	/* a fifo whose buffer is allocated at run time */
	DECLARE_KFIFO_PTR(dyn, char);

	char buf[16];
	unsigned int n;
	int v, err = 0;
	bool ok = true;

	/* a byte stream: copies as much as fits and returns how much */
	dump_bytes("empty:");
	n = kfifo_in(&bytes, "abcde", 5);
	dump_bytes("in \"abcde\":");

	n = kfifo_out(&bytes, buf, 3);
	buf[n] = '\0';
	pr_info("out 3: \"%s\"\n", buf);
	dump_bytes("after out:");
	ok &= !strcmp(buf, "abc");

	/* the write wraps around the end of the buffer */
	n = kfifo_in(&bytes, "fghijk", 6);
	dump_bytes("in \"fghijk\", wraps:");
	ok &= n == 6 && kfifo_is_full(&bytes);

	n = kfifo_in(&bytes, "xyz", 3);
	pr_info("in \"xyz\" when full: %u copied\n", n);
	ok &= n == 0;

	/* look without taking */
	if (kfifo_peek(&bytes, buf))
		pr_info("peek: '%c', len still %u\n", buf[0], kfifo_len(&bytes));

	n = kfifo_out(&bytes, buf, sizeof(buf) - 1);
	buf[n] = '\0';
	pr_info("out all: \"%s\"\n", buf);
	dump_bytes("drained:");
	ok &= !strcmp(buf, "defghijk") && kfifo_is_empty(&bytes);

	/* typed: one element at a time with put and get */
	for (v = 10; v <= 50; v += 10)
		if (!kfifo_put(&ints, v))
			pr_info("put %d: full\n", v);
	pr_info("ints: size %u, len %u\n", kfifo_size(&ints), kfifo_len(&ints));
	while (kfifo_get(&ints, &v))
		pr_info("get %d\n", v);

	/*
	 * Record input is all-or-nothing when space is insufficient, unlike
	 * the byte stream. A large enough output buffer gets the whole record.
	 */
	INIT_KFIFO(recs);
	kfifo_in(&recs, "hello", 5);
	kfifo_in(&recs, "hi", 2);
	kfifo_in(&recs, "kernel", 6);
	pr_info("records: %u bytes used for 13 bytes of data\n",
		kfifo_len(&recs));
	while (!kfifo_is_empty(&recs)) {
		pr_info("next record is %u bytes\n", kfifo_peek_len(&recs));
		n = kfifo_out(&recs, buf, sizeof(buf) - 1);
		buf[n] = '\0';
		pr_info("record: \"%s\"\n", buf);
	}
	ok &= !strcmp(buf, "kernel");

	/* A short destination truncates the record and discards its rest. */
	kfifo_in(&recs, "hello", 5);
	kfifo_in(&recs, "hi", 2);
	n = kfifo_out(&recs, buf, 3);
	buf[n] = '\0';
	pr_info("short record read: \"%s\", next length %u\n", buf,
		kfifo_peek_len(&recs));
	ok &= n == 3 && !strcmp(buf, "hel") && kfifo_peek_len(&recs) == 2;
	n = kfifo_out(&recs, buf, sizeof(buf) - 1);
	buf[n] = '\0';
	ok &= n == 2 && !strcmp(buf, "hi") && kfifo_is_empty(&recs);

	/* kfifo_alloc() rounds the size up to a power of 2 */
	err = kfifo_alloc(&dyn, 100, GFP_KERNEL);
	if (err)
		return err;
	pr_info("kfifo_alloc 100: size %u\n", kfifo_size(&dyn));
	ok &= kfifo_size(&dyn) == 128;
	kfifo_free(&dyn);

	if (!ok) {
		pr_err("kfifo check failed\n");
		return -EINVAL;
	}
	pr_info("kfifo check passed\n");
	return 0;
}

static int __init mod_init(void)
{
	return kfifo_demo();
}

/* All resources have been released before kfifo_demo() returns. */
static void __exit mod_exit(void)
{
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Demonstrating the kernel's kfifo");
