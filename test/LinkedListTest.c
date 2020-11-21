#include "test.h"
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
