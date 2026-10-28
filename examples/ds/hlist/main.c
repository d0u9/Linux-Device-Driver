// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/hashtable.h>
#include <linux/slab.h>

/* key 1 is 'A', key 2 is 'B', ... */
struct item {
	int key;
	char val;
	struct hlist_node node;
};

/*
 * 8 buckets (2^3), each a struct hlist_head: one pointer, @first, where a
 * list_head would be two. The table is the reason hlist exists: a hash
 * table has many buckets, most of them empty or short.
 *
 * Ordinary hash helpers do not lock; concurrent access needs external
 * synchronization. RCU traversal additionally needs RCU-aware updates
 * and deferred object reclamation. _safe only supports local deletion.
 *
 * DEFINE_HASHTABLE() is an array of hlist_head with every bucket NULL:
 *	static struct hlist_head table[8] = { [0 ... 7] = HLIST_HEAD_INIT };
 */
static DEFINE_HASHTABLE(table, 3);

/* hash_add() prepends to a bucket; it does not reject duplicate keys. */
static int insert(int key)
{
	struct item *it = kzalloc_obj(*it);

	if (!it)
		return -ENOMEM;
	it->key = key;
	it->val = 'A' + key - 1;
	hash_add(table, &it->node, it->key);
	/* without the helper: hlist_add_head(&it->node, &table[hash_min(key, 3)]); */
	return 0;
}

/* only the bucket @key hashes to is walked; other keys may share it */
static struct item *search(int key)
{
	struct item *it;

	hash_for_each_possible(table, it, node, key)
		if (it->key == key)
			return it;
	return NULL;
}

static void dump_table(const char *title)
{
	struct item *it;
	char line[128];
	int bkt, len;

	pr_info("== %s ==\n", title);
	for (bkt = 0; bkt < HASH_SIZE(table); bkt++) {
		len = 0;
		hlist_for_each_entry(it, &table[bkt], node)
			len += scnprintf(line + len, sizeof(line) - len,
					 " -> %d:%c", it->key, it->val);
		pr_info("  [%d]%s -> NULL\n", bkt, len ? line : "");
	}
}

static void destroy_table(void)
{
	struct hlist_node *tmp;
	struct item *it;
	int bkt;

	hash_for_each_safe(table, bkt, tmp, it, node) {
		hash_del(&it->node);
		kfree(it);
	}
}

static int count_table(void)
{
	struct item *it;
	int bkt, n = 0;

	hash_for_each(table, bkt, it, node)
		n++;
	return n;
}

static int __init hlist_demo(void)
{
	struct item *it;
	int key, err = 0;

	pr_info("sizeof list_head %zu, hlist_head %zu: a 1024-bucket table is %zu or %zu bytes\n",
		sizeof(struct list_head), sizeof(struct hlist_head),
		1024 * sizeof(struct list_head),
		1024 * sizeof(struct hlist_head));

	for (key = 1; key <= 16; key++) {
		err = insert(key);
		if (err)
			goto out;
	}
	dump_table("keys 1..16");

	it = search(5);
	pr_info("search 5: %c, search 100: %s\n", it ? it->val : '-',
		search(100) ? "found" : "not found");

	/*
	 * hlist_node's pprev points at the pointer that points at this node:
	 * the head's @first for the first node, the previous node's @next
	 * otherwise. So a node is unlinked in O(1) without knowing its
	 * bucket and without special-casing the first node.
	 */
	it = search(5);
	if (it) {
		pr_info("5's pprev is %s\n",
			it->node.pprev == &table[hash_min(5, 3)].first ?
			"its bucket's first" : "the previous node's next");
		hash_del(&it->node);	/* hlist_del_init(): pprev = NULL */
		pr_info("5 deleted, hashed: %s\n",
			hash_hashed(&it->node) ? "yes" : "no");
		kfree(it);
	}
	dump_table("key 5 deleted");

	if (count_table() != 15 || search(5) || !search(6) ||
	    search(6)->val != 'F') {
		pr_err("hash table check failed\n");
		err = -EINVAL;
		goto out;
	}
	pr_info("hash table check passed\n");

out:
	destroy_table();
	return err;
}

static int __init mod_init(void)
{
	return hlist_demo();
}

/* All resources have been released before hlist_demo() returns. */
static void __exit mod_exit(void)
{
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Demonstrating hlist through the kernel's hash table");
