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
void list_free(list_t *list);

void list_append(list_t *list, node_t *v);
node_t *list_insert(list_t *list, void *v);
void list_remove(list_t *list, void *v);
void *list_dequeue(list_t *list);
void list_delete(list_t *list, node_t *v);

#define list_foreach(v, list) for list_each((v), (list))
#define list_each(v, list) (node_t *v = (list)->head; v != NULL; v = v->next)

#endif
