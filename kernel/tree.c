#include <assert.h>

#include <heap.h>
#include <tree.h>

tree_t *tree_new() {
	return kcalloc(1, sizeof(tree_t));
}

tree_node_t *tree_node_new() {
	tree_node_t *v = kcalloc(1, sizeof(tree_node_t));
	if (v == NULL) {
		return NULL;
	}

	v->children = list_init();

	return v;
}

void tree_node_insert_child(tree_t *tree, tree_node_t *parent, tree_node_t *node) {
	(void)tree;
	list_insert(parent->children, node);
}

tree_node_t *tree_node_find(tree_node_t *parent, void *value) {
	if (parent->value == value) {
		return parent;
	}

	node_t *node = parent->children->head;
	while (node != NULL) {
		tree_node_t *found = tree_node_find(node->value, value);
		if (found != NULL) {
			return found;
		}
		node = node->next;
	}
	return NULL;
}

tree_node_t *tree_find(tree_t *tree, void *value) {
	return tree_node_find(tree->root, value);
}

void tree_node_delete_child(tree_t *tree, tree_node_t *parent, tree_node_t *node) {
	(void)tree;
	list_remove(parent->children, node);
}
