#ifndef LIST_H_
#define LIST_H_

#include <stdlib.h>

struct listnode {
	void *data;
	struct listnode *next;
};

struct list {
	struct listnode *head;
	struct listnode *tail;
};

static inline int list_init(struct list *l)
{
	l->head = l->tail = NULL;
	return 0;
}

static inline int list_insert(struct list *l, void *data)
{
	struct listnode *n = malloc(sizeof(*n));
	if (!n)
		return 1;
	n->data = data;
	n->next = l->head;
	l->head = n;
	return 0;
}

static inline int list_append(struct list *l, void *data)
{
	struct listnode *n = malloc(sizeof(*n));
	if (!n)
		return 1;
	n->data = data;
	n->next = NULL;
	if (l->tail)
		l->tail->next = n;
	else
		l->head = n;
	l->tail = n;
	return 0;
}

static inline void list_destroy(struct list *l)
{
	struct listnode *n = l->head;
	if (!n)
		return;
	struct listnode *nn = n->next;
	while (nn) {
		free(n);
		n = nn;
		nn = n->next;
	}
	free(n);
	l->head = l->tail = NULL;
}

#endif
