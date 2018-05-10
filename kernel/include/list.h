#ifndef LIST_H
#define LIST_H 1

#include <stdint.h>
#include <stddef.h>


typedef struct node {
	struct node *next;
	struct node *prev;
	void *value;
	void *owner;
} node_t;

typedef struct {
	node_t *head;
	node_t *tail;
	size_t length;
} list_t;

list_t *list_init();
void list_append(list_t *list, node_t *v);
void list_free(list_t *list);
node_t *list_insert(list_t *list, void *v);
void list_delete(list_t *list, node_t *v);

#endif
