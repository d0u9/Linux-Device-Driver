// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/rbtree.h>
#include <linux/rbtree_augmented.h>	/* rb_is_red(), for drawing only */
#include <linux/slab.h>

/* A map from int keys to letters: key 1 is 'A', key 2 is 'B', ... */
struct rb_data {
	int key;
	char val;
	struct rb_node rb;
};

static struct rb_root demo_root = RB_ROOT;

/*
 * The kernel's rbtree only balances; it knows nothing of keys. The order
 * comes from the driver, as a comparison function: <0, 0 or >0, like
 * strcmp().
 */
static int rb_data_cmp(struct rb_node *a, const struct rb_node *b)
{
	int ka = rb_entry(a, struct rb_data, rb)->key;
	int kb = rb_entry(b, struct rb_data, rb)->key;

	return (ka > kb) - (ka < kb);
}

/* The same order, comparing a bare key with a node, for rb_find(). */
static int rb_data_key_cmp(const void *key, const struct rb_node *node)
{
	int k = *(const int *)key;
	int nk = rb_entry(node, struct rb_data, rb)->key;

	return (k > nk) - (k < nk);
}

/*
 * Insert @new unless its key is already there. rb_find_add() walks down
 * to where the key belongs and links the node there, or returns the node
 * that has the key. rb_add() is the same without the check, for trees
 * that keep duplicate keys.
 *
 * Returns false, without inserting, if the key is taken.
 */
static bool insert(struct rb_root *root, struct rb_data *new)
{
	return !rb_find_add(&new->rb, root, rb_data_cmp);
}

/*
 * Before rb_find_add() (v5.12), every user wrote the walk by hand:
 *
 * static bool insert(struct rb_root *root, struct rb_data *new)
 * {
 *	struct rb_node **link = &root->rb_node, *parent = NULL;
 *	struct rb_data *data;
 *
 *	while (*link) {
 *		parent = *link;
 *		data = rb_entry(parent, struct rb_data, rb);
 *
 *		if (new->key < data->key)
 *			link = &parent->rb_left;
 *		else if (new->key > data->key)
 *			link = &parent->rb_right;
 *		else
 *			return false;
 *	}
 *
 *	// hang the node at the empty link, then recolor and rotate
 *	rb_link_node(&new->rb, parent, link);
 *	rb_insert_color(&new->rb, root);
 *	return true;
 * }
 */

static struct rb_data *search(struct rb_root *root, int key)
{
	struct rb_node *node = rb_find(&key, root, rb_data_key_cmp);

	return node ? rb_entry(node, struct rb_data, rb) : NULL;
}

/*
 * Before rb_find() (v5.12):
 *
 * static struct rb_data *search(struct rb_root *root, int key)
 * {
 *	struct rb_node *node = root->rb_node;
 *	struct rb_data *data;
 *
 *	while (node) {
 *		data = rb_entry(node, struct rb_data, rb);
 *
 *		if (key < data->key)
 *			node = node->rb_left;
 *		else if (key > data->key)
 *			node = node->rb_right;
 *		else
 *			return data;
 *	}
 *	return NULL;
 * }
 */

/*
 * Draw the tree on its side, root on the left, larger keys above: right
 * subtree first, then the node, then the left subtree. Red nodes are
 * marked; the rest are black. Reading the colors needs
 * <linux/rbtree_augmented.h>, which ordinary users do not include.
 */
static void draw_subtree(struct rb_node *node, int depth)
{
	struct rb_data *data;

	if (!node)
		return;

	draw_subtree(node->rb_right, depth + 1);
	data = rb_entry(node, struct rb_data, rb);
	pr_info("  %*s%d:%c%s\n", depth * 6, "", data->key, data->val,
		rb_is_red(node) ? " (red)" : "");
	draw_subtree(node->rb_left, depth + 1);
}

/* In key order: rb_first() is the smallest, rb_next() the one after. */
static void dump_tree(struct rb_root *root, const char *title)
{
	struct rb_data *data;
	struct rb_node *node;
	char line[128];
	int len = 0;

	for (node = rb_first(root); node; node = rb_next(node)) {
		data = rb_entry(node, struct rb_data, rb);
		len += scnprintf(line + len, sizeof(line) - len, " %d:%c",
				 data->key, data->val);
	}

	pr_info("== %s ==\n", title);
	pr_info("  in order:%s\n", len ? line : " (empty)");
	draw_subtree(root->rb_node, 0);
}

/*
 * Check the tree is in key order and count its nodes. Returns the count,
 * or -1 if the order is broken.
 */
static int check_tree(struct rb_root *root)
{
	struct rb_data *data, *prev = NULL;
	struct rb_node *node;
	int n = 0;

	for (node = rb_first(root); node; node = rb_next(node)) {
		data = rb_entry(node, struct rb_data, rb);
		if (prev && prev->key >= data->key)
			return -1;
		prev = data;
		n++;
	}
	return n;
}

/*
 * Free every node. The postorder walk visits children before their
 * parent, so a node can be freed as soon as it is visited; the tree is
 * not rebalanced on the way (no rb_erase()), and is empty afterwards.
 */
static void destroy_tree(struct rb_root *root)
{
	struct rb_data *cur, *tmp;

	rbtree_postorder_for_each_entry_safe(cur, tmp, root, rb)
		kfree(cur);
	*root = RB_ROOT;
}

static int insert_range(struct rb_root *root, int first, int last)
{
	struct rb_data *data;
	int key;

	for (key = first; key <= last; key++) {
		data = kzalloc_obj(*data);
		if (!data)
			return -ENOMEM;
		data->key = key;
		data->val = 'A' + key - 1;
		if (!insert(root, data))
			kfree(data);
	}
	return 0;
}

static void show_search(struct rb_root *root, int key)
{
	struct rb_data *data = search(root, key);

	if (data)
		pr_info("search %d: '%c'\n", key, data->val);
	else
		pr_info("search %d: not found\n", key);
}

static int __init mod_init(void)
{
	struct rb_root *root = &demo_root;
	struct rb_data *old, *new, *dup;
	int err;

	err = insert_range(root, 1, 6);
	if (err)
		goto out;
	dump_tree(root, "keys 1..6");

	err = insert_range(root, 7, 11);
	if (err)
		goto out;
	dump_tree(root, "keys 7..11 added");

	/* a key that is already there is refused, and the node is ours */
	dup = kzalloc_obj(*dup);
	if (!dup) {
		err = -ENOMEM;
		goto out;
	}
	dup->key = 3;
	dup->val = '?';
	if (!insert(root, dup)) {
		pr_info("insert 3 again: refused\n");
		kfree(dup);
	}

	show_search(root, 100);
	show_search(root, 2);

	/* replace the node of key 2: same key, so the same place in the tree */
	old = search(root, 2);
	new = kzalloc_obj(*new);
	if (!old || !new) {
		kfree(new);
		err = -ENOMEM;
		goto out;
	}
	new->key = 2;
	new->val = 'z';
	rb_replace_node(&old->rb, &new->rb, root);
	kfree(old);
	show_search(root, 2);
	if (search(root, 2) != new) {
		pr_err("replaced node not found\n");
		err = -EINVAL;
		goto out;
	}

	/* erase it: unlinked and rebalanced, then it is ours to free */
	rb_erase(&new->rb, root);
	kfree(new);
	show_search(root, 2);
	dump_tree(root, "key 2 erased");

	/* 1..11 less key 2, in order, 'C' still at 3 */
	if (check_tree(root) != 10 || search(root, 2) ||
	    !search(root, 3) || search(root, 3)->val != 'C') {
		pr_err("tree check failed\n");
		err = -EINVAL;
		goto out;
	}
	pr_info("tree check passed\n");

out:
	destroy_tree(root);
	return err;
}

static void __exit mod_exit(void)
{
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Demonstrating the kernel's rbtree");
