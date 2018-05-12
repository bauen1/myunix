#include <console.h>

#include <assert.h>
#include <stddef.h>

#include <cpu.h>
#include <ringbuffer.h>
#include <process.h>
#include <heap.h>
#include <list.h>

list_t *list_init() {
	list_t *v = kcalloc(1, sizeof(list_t));
	return v;
}

void list_free(list_t *list) {
	node_t *v = list->head;
	while (v) {
		node_t *v2 = v->next;
		kfree(v);
		v = v2;
	}
}


void list_append(list_t *list, node_t *node) {
	assert(node->owner == NULL);
	node->owner = list;

	if (list->length == 0) {
		list->head = node;
		list->tail = node;
		node->prev = NULL;
		node->next = NULL;
	} else {
		list->tail->next = node;
		node->prev = list->tail;
		list->tail = node;
	}
	list->length++;
}

node_t *list_insert(list_t *list, void *v) {
	node_t *node = kcalloc(1, sizeof(node_t));
	node->value = v;
	list_append(list, node);
	return node;
}

void list_remove(list_t *list, void *v) {
	for (node_t *i = list->head; i != NULL; i = i->next) {
		if (i->value == v) {
			list_delete(list, i);
			kfree(i);
			break;
		}
	}
}

void list_delete(list_t *list, node_t *v) {
	assert(v->owner == list);
	if (v == list->head) {
		list->head = v->next;
	}
	if (v == list->tail) {
		list->tail = v->prev;
	}

	if (v->prev) {
		v->prev->next = v->next;
	}
	if (v->next) {
		v->next->prev = v->prev;
	}
	v->prev = NULL;
	v->next = NULL;
	v->owner = NULL;
	list->length--;
}
