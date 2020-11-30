/*
FreeDOS-32 kernel
Copyright (C) 2008-2020  Salvatore ISAJA

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#ifndef PRIORITYQUEUE_H_INCLUDED
#define PRIORITYQUEUE_H_INCLUDED

#include "Types.h"

static inline unsigned PriorityQueueNode_getKey(const PriorityQueueImpl_Node *n) {
    return ((const PriorityQueueNode *) n)->key;
}

static inline void PriorityQueue_init(PriorityQueue *queue) {
    PriorityQueueImpl_initialize(&queue->impl, 8);
    queue->min = NULL;
}

static inline bool PriorityQueue_isEmpty(const PriorityQueue *queue) {
    return PriorityQueueImpl_isEmpty(&queue->impl);
}

static inline void PriorityQueue_insert(PriorityQueue *queue, PriorityQueueNode *x) {
    PriorityQueueImpl_insert(&queue->impl, &x->n, false);
    if (queue->min == NULL || x->key < queue->min->key) {
        queue->min = x;
    }
}

static inline void PriorityQueue_insertFront(PriorityQueue *queue, PriorityQueueNode *x) {
    PriorityQueueImpl_insert(&queue->impl, &x->n, true);
    if (queue->min == NULL || x->key <= queue->min->key) {
        queue->min = x;
    }
}

static inline PriorityQueueNode *PriorityQueue_peek(PriorityQueue *queue) {
    assert(!PriorityQueue_isEmpty(queue));
    return queue->min;
}

static inline PriorityQueueNode *PriorityQueue_poll(PriorityQueue *queue) {
    PriorityQueueNode *result = queue->min;
    PriorityQueueImpl_remove(&queue->impl, &result->n);
    queue->min = !PriorityQueueImpl_isEmpty(&queue->impl) ? (PriorityQueueNode *) PriorityQueueImpl_findMin(&queue->impl) : NULL;
    return result;
}

static inline void PriorityQueue_remove(PriorityQueue *queue, PriorityQueueNode *x) {
    PriorityQueueImpl_remove(&queue->impl, &x->n);
    if (x == queue->min) {
        queue->min = !PriorityQueueImpl_isEmpty(&queue->impl) ? (PriorityQueueNode *) PriorityQueueImpl_findMin(&queue->impl) : NULL;
    }
}

static inline PriorityQueueNode *PriorityQueue_pollAndInsert(PriorityQueue *queue, PriorityQueueNode *newNode, bool front) {
    PriorityQueueNode *result = queue->min;
    PriorityQueueImpl_remove(&queue->impl, &result->n);
    PriorityQueueImpl_insert(&queue->impl, &newNode->n, front);
    queue->min = !PriorityQueueImpl_isEmpty(&queue->impl) ? (PriorityQueueNode *) PriorityQueueImpl_findMin(&queue->impl) : NULL;
    return result;
}

#endif
