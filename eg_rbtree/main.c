#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/rbtree.h>
#include <linux/slab.h>

struct rb_data {
	int key;
	int val;
	struct rb_node rb;
};

struct rb_root _root = RB_ROOT;

/*
 * Have to manually implement this insert function
 */
static
void insert(struct rb_root *root, struct rb_data *new)
{
	int new_key = new->key;
	struct rb_node **link = &root->rb_node, *parent = NULL;
	struct rb_data *data;

	while(*link) {
		parent = *link;
		data = rb_entry(parent, struct rb_data, rb);

		if (new_key > data->key)
			link = &(*link)->rb_right;
		else if (new_key < data->key)
			link = &(*link)->rb_left;
		else
			return;

	}

	rb_link_node(&new->rb, parent, link);
	rb_insert_color(&new->rb, root);
}

/*
 * Have to manually impelement this search function
 */
static
struct rb_data *search(struct rb_root *root, int target)
{
	struct rb_node *node;
	struct rb_data *data;

	if (!root)
		return NULL;

	node = root->rb_node;

	while (node) {
		data = rb_entry(node, struct rb_data, rb);

		if (target > data->val)
			node = node->rb_right;
		else if (target < data->val)
			node = node->rb_left;
		else
			return data;
	}

	return NULL;
}

static
void dump_tree(struct rb_root *root, const char *str)
{
	struct rb_data *cur, *tmp;
	
	pr_info("======= %s ========\n", str);
	pr_info("======= root: %d  ========\n",
		rb_entry(root->rb_node, struct rb_data, rb)->val);

	rbtree_postorder_for_each_entry_safe(cur, tmp, root, rb) {
		pr_info("= key: %d, val: %d\n", cur->key, cur->val);
	}
	pr_info("--------------------\n");
}

static
void destroy_tree(struct rb_root *root)
{
	struct rb_data *cur, *tmp;
	
	pr_info("======= Destroying ========\n");
	pr_info("======= root: %d  ========\n",
		rb_entry(root->rb_node, struct rb_data, rb)->val);

	rbtree_postorder_for_each_entry_safe(cur, tmp, root, rb) {
		rb_erase(&cur->rb, root);
		kfree(cur);
	}
	pr_info("--------------------\n");
}


static
void insert_arr(struct rb_root *root, int arr[][2], int len)
{
	int i;
	struct rb_data *data;

	for (i = 0; i < len; ++i) {
		data = kzalloc(sizeof(struct rb_data), GFP_KERNEL);
		data->key = arr[i][0];
		data->val = arr[i][1];
		insert(root, data);
	}
}

static
int __init mod_init(void)
{
	struct rb_root *root = &_root;
	struct rb_data *data, *old, *new;
	int nums0[][2] = { {1,1}, {2,2}, {3,3}, {4,4}, {5,5}, {6,6} };
	int nums1[][2] = { {7,7}, {8,8} ,{9,9}, {10,10}, {11,11} };

	pr_info("Hello, world!\n");

	insert_arr(root, nums0, ARRAY_SIZE(nums0));
	dump_tree(root, "Init tree");

	insert_arr(root, nums1, ARRAY_SIZE(nums1));
	dump_tree(root, "Insert { 7, 8 ,9, 10, 11 }");

	pr_info("Search for key 100\n");
	data = search(root, 100);
	if (data)
		pr_info("Search result: key: %d, val: %d\n",
			data->key, data->val);
	else
		pr_info("Search result: NULL\n");

	pr_info("Search for key == 2\n");
	data = search(root, 2);
	if (data)
		pr_info("Search result: key: %d, val: %d\n",
			data->key, data->val);
	else
		pr_info("Search result: NULL\n");


	pr_info("Update val of data with key == 2 to 100\n");
	old = data;
	new = kzalloc(sizeof(struct rb_data), GFP_KERNEL);
	new->key = 2;
	new->val = 100;
	rb_replace_node(&old->rb, &new->rb, root);
	kfree(old);
	dump_tree(root, "dump tree after replacing");

	pr_info("Search for key == 2, after replacing\n");
	data = search(root, 2);
	if (data)
		pr_info("Search result: key: %d, val: %d\n",
			data->key, data->val);
	else
		pr_info("Search result: NULL\n");


	pr_info("delete key == 2 from tree\n");
	rb_erase(&new->rb, root);

	pr_info("Search for key == 2, after erasing\n");
	data = search(root, 2);
	if (data)
		pr_info("Search result: key: %d, val: %d\n",
			data->key, data->val);
	else
		pr_info("Search result: NULL\n");
	dump_tree(root, "dump tree after erasing");


	pr_info("Destroy the tree\n");
	destroy_tree(root);

	pr_info("root->rb_node == %p\n", root->rb_node);

	return 0;
}

static
void __exit mod_exit(void)
{
	pr_info("Bye, World!\n");
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d0u9");
MODULE_DESCRIPTION("Demostrating the using of rb tree.");
