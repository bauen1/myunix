#ifndef TREE_H
#define TREE_H 1

#include <list.h>

typedef struct tree_node {
	void *value;
	list_t *children;
	struct tree_node *parent;
} tree_node_t;

typedef struct tree {
	tree_node_t *root;
} tree_t;

tree_t *tree_new();
tree_node_t *tree_node_new();
void tree_node_insert_child(tree_t *tree, tree_node_t *parent, tree_node_t *node);
tree_node_t *tree_node_find(tree_node_t *parent, void *value);
tree_node_t *tree_find(tree_t *tree, void *value);
void tree_node_delete_child(tree_t *tree, tree_node_t *parent, tree_node_t *node);

#endif
