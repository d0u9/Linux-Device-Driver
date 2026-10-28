// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/slab.h>

#define NSLOTS	40

/*
 * %*pbl prints the set bits as ranges, "0-3,8"; %*pb as hex. Both take
 * the number of bits as the width.
 */
static void dump(const unsigned long *slots, const char *title)
{
	pr_info("%-26s %*pbl  (hex %*pb, %u set)\n", title, NSLOTS, slots,
		NSLOTS, slots, bitmap_weight(slots, NSLOTS));
}

/*
 * Take the first free slot. Two callers may find the same zero bit, so
 * the bit is claimed with test_and_set_bit(), which is atomic: only one
 * sees it was 0, and the other looks again. Claiming a bit alone does
 * not publish the associated object: its lifetime and publication need
 * a separate synchronization protocol.
 */
static int take_slot(unsigned long *slots)
{
	unsigned int bit;

	do {
		bit = find_first_zero_bit(slots, NSLOTS);
		if (bit >= NSLOTS)
			return -ENOSPC;
	} while (test_and_set_bit(bit, slots));
	return bit;
}

/*
 * 40 bits, rounded up to whole unsigned longs: one on a 64-bit kernel,
 * two on a 32-bit kernel. The bits past 40 are there but never used;
 * the bitmap_*() calls take the size.
 *	unsigned long slots[BITS_TO_LONGS(NSLOTS)];
 */
static int __init bitmap_demo(void)
{
	DECLARE_BITMAP(slots, NSLOTS) = { 0 };
	unsigned long *big;
	unsigned int bit, start;
	int i, n;
	bool ok = true;

	dump(slots, "empty:");

	/*
	 * set_bit() is atomic, safe against others changing the same word;
	 * __set_bit() is a plain read-modify-write, for a bitmap nobody
	 * else touches yet, or under a lock.
	 */
	set_bit(0, slots);
	__set_bit(5, slots);
	dump(slots, "set 0 and 5:");

	for (i = 0; i < 4; i++)
		pr_info("take_slot: %d\n", take_slot(slots));
	dump(slots, "4 slots taken:");
	ok &= bitmap_weight(slots, NSLOTS) == 6;

	clear_bit(2, slots);
	dump(slots, "2 given back:");
	ok &= take_slot(slots) == 2;

	/*
	 * Range operations are non-atomic; concurrent users need a lock,
	 * even when other callers use atomic single-bit operations. A failed
	 * search may return beyond NSLOTS: check before setting the range.
	 */
	start = bitmap_find_next_zero_area(slots, NSLOTS, 0, 8, 0);
	pr_info("8 free in a row from %u\n", start);
	if (start > NSLOTS - 8) {
		pr_err("no room for an 8-bit range\n");
		return -EINVAL;
	}
	bitmap_set(slots, start, 8);
	dump(slots, "range set:");
	ok &= start == 6;

	n = 0;
	for_each_set_bit(bit, slots, NSLOTS)
		n++;
	pr_info("for_each_set_bit: %d bits\n", n);

	/* fill up, then one more */
	bitmap_fill(slots, NSLOTS);
	i = take_slot(slots);
	pr_info("full, take_slot: %d\n", i);
	ok &= i == -ENOSPC;
	ok &= bitmap_full(slots, NSLOTS);
	bitmap_zero(slots, NSLOTS);

	/* a size known only at run time */
	big = bitmap_zalloc(1000, GFP_KERNEL);
	if (!big)
		return -ENOMEM;
	bitmap_set(big, 100, 50);
	pr_info("1000 bits allocated: %*pbl\n", 1000, big);
	ok &= bitmap_weight(big, 1000) == 50;
	bitmap_free(big);

	if (!ok) {
		pr_err("bitmap check failed\n");
		return -EINVAL;
	}
	pr_info("bitmap check passed\n");
	return 0;
}

static int __init mod_init(void)
{
	return bitmap_demo();
}

/* All resources have been released before bitmap_demo() returns. */
static void __exit mod_exit(void)
{
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Demonstrating the kernel's bitmap");
