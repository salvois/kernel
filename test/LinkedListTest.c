/*
Intrusive circular doubly linked list collection.
Copyright 2009-2020 Salvatore ISAJA. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED THE COPYRIGHT HOLDER ``AS IS'' AND ANY EXPRESS
OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "test.h"
#include "stddef.h"
#include "LinkedList.h"

static void LinkedListTest_empty() {
    LinkedList_Node sentinel;
    LinkedList_initialize(&sentinel);
    ASSERT(sentinel.prev == &sentinel);
    ASSERT(sentinel.next == &sentinel);
}

static void LinkedListTest_insertMany() {
    LinkedList_Node sentinel;
    LinkedList_Node node1;
    LinkedList_Node node2;
    LinkedList_Node node3;
    LinkedList_initialize(&sentinel);
    LinkedList_insertAfter(&node1, &sentinel);
    LinkedList_insertAfter(&node2, &node1);
    LinkedList_insertAfter(&node3, &node2);
    
    ASSERT(sentinel.next == &node1);
    ASSERT(node1.next == &node2);
    ASSERT(node2.next == &node3);
    ASSERT(node3.next == &sentinel);
    
    ASSERT(sentinel.prev == &node3);
    ASSERT(node3.prev == &node2);
    ASSERT(node2.prev == &node1);
    ASSERT(node1.prev == &sentinel);
}

static void LinkedListTest_insertAfter() {
    LinkedList_Node sentinel;
    LinkedList_Node node1;
    LinkedList_Node node2;
    LinkedList_Node node3;
    LinkedList_Node node4;
    LinkedList_initialize(&sentinel);
    LinkedList_insertAfter(&node1, &sentinel);
    LinkedList_insertAfter(&node2, &node1);
    LinkedList_insertAfter(&node4, &node2);
    
    LinkedList_insertAfter(&node3, &node2);
    
    ASSERT(sentinel.next == &node1);
    ASSERT(node1.next == &node2);
    ASSERT(node2.next == &node3);
    ASSERT(node3.next == &node4);
    ASSERT(node4.next == &sentinel);
    
    ASSERT(sentinel.prev == &node4);
    ASSERT(node4.prev == &node3);
    ASSERT(node3.prev == &node2);
    ASSERT(node2.prev == &node1);
    ASSERT(node1.prev == &sentinel);
}

static void LinkedListTest_insertBefore() {
    LinkedList_Node sentinel;
    LinkedList_Node node1;
    LinkedList_Node node2;
    LinkedList_Node node3;
    LinkedList_Node node4;
    LinkedList_initialize(&sentinel);
    LinkedList_insertAfter(&node1, &sentinel);
    LinkedList_insertAfter(&node2, &node1);
    LinkedList_insertAfter(&node4, &node2);
    
    LinkedList_insertBefore(&node3, &node4);
    
    ASSERT(sentinel.next == &node1);
    ASSERT(node1.next == &node2);
    ASSERT(node2.next == &node3);
    ASSERT(node3.next == &node4);
    ASSERT(node4.next == &sentinel);
    
    ASSERT(sentinel.prev == &node4);
    ASSERT(node4.prev == &node3);
    ASSERT(node3.prev == &node2);
    ASSERT(node2.prev == &node1);
    ASSERT(node1.prev == &sentinel);
}

static void LinkedListTest_remove() {
    LinkedList_Node sentinel;
    LinkedList_Node node1;
    LinkedList_Node node2;
    LinkedList_Node node3;
    LinkedList_Node node4;
    LinkedList_initialize(&sentinel);
    LinkedList_insertAfter(&node1, &sentinel);
    LinkedList_insertAfter(&node2, &node1);
    LinkedList_insertAfter(&node3, &node2);
    LinkedList_insertAfter(&node4, &node3);
    
    LinkedList_remove(&node2);
    
    ASSERT(sentinel.next == &node1);
    ASSERT(node1.next == &node3);
    ASSERT(node3.next == &node4);
    ASSERT(node4.next == &sentinel);
    
    ASSERT(sentinel.prev == &node4);
    ASSERT(node4.prev == &node3);
    ASSERT(node3.prev == &node1);
    ASSERT(node1.prev == &sentinel);
    
    ASSERT(node2.prev == NULL);
    ASSERT(node2.next == NULL);
}

static void LinkedListTest_removeAll() {
    LinkedList_Node sentinel;
    LinkedList_Node node1;
    LinkedList_Node node2;
    LinkedList_initialize(&sentinel);
    LinkedList_insertAfter(&node1, &sentinel);
    LinkedList_insertAfter(&node2, &node1);
    
    LinkedList_remove(&node2);
    LinkedList_remove(&node1);
    
    ASSERT(sentinel.prev == &sentinel);
    ASSERT(sentinel.next == &sentinel);
}

void LinkedListTest_run() {
    RUN_TEST(LinkedListTest_empty);
    RUN_TEST(LinkedListTest_insertMany);
    RUN_TEST(LinkedListTest_insertAfter);
    RUN_TEST(LinkedListTest_insertBefore);
    RUN_TEST(LinkedListTest_remove);
    RUN_TEST(LinkedListTest_removeAll);
}
