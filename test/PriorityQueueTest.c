#include "test.h"
#include "kernel.h"

static void PriorityQueueTest_new() {
    PriorityQueue pq;
    PriorityQueue_init(&pq);

    ASSERT(PriorityQueue_isEmpty(&pq) == true);
}

static void PriorityQueueTest_insertSingle() {
    PriorityQueue pq;
    PriorityQueue_init(&pq);
    PriorityQueueNode node = { .key = 42 };
    
    PriorityQueue_insert(&pq, &node);

    ASSERT(PriorityQueue_isEmpty(&pq) == false);
    ASSERT(PriorityQueue_peek(&pq) == &node);
}

static void PriorityQueueTest_insertFrontSingle() {
    PriorityQueue pq;
    PriorityQueue_init(&pq);
    PriorityQueueNode node = { .key = 42 };
    
    PriorityQueue_insertFront(&pq, &node);

    ASSERT(PriorityQueue_isEmpty(&pq) == false);
    ASSERT(PriorityQueue_peek(&pq) == &node);
}

static void PriorityQueueTest_pollSingle() {
    PriorityQueue pq;
    PriorityQueue_init(&pq);
    PriorityQueueNode node = { .key = 42 };
    PriorityQueue_insert(&pq, &node);
    
    PriorityQueueNode *polled = PriorityQueue_poll(&pq);

    ASSERT(PriorityQueue_isEmpty(&pq) == true);
    ASSERT(polled == &node);
}

static void PriorityQueueTest_removeSingle() {
    PriorityQueue pq;
    PriorityQueue_init(&pq);
    PriorityQueueNode node = { .key = 42 };
    PriorityQueue_insert(&pq, &node);
    
    PriorityQueue_remove(&pq, &node);

    ASSERT(PriorityQueue_isEmpty(&pq) == true);
}

static void PriorityQueueTest_pollAndInsertSingle() {
    PriorityQueue pq;
    PriorityQueue_init(&pq);
    PriorityQueueNode node = { .key = 42 };
    PriorityQueueNode anotherNode = { .key = 100 };
    PriorityQueue_insert(&pq, &node);
    
    PriorityQueueNode *polled = PriorityQueue_pollAndInsert(&pq, &anotherNode, false);

    ASSERT(polled == &node);
    ASSERT(PriorityQueue_isEmpty(&pq) == false);
    ASSERT(PriorityQueue_peek(&pq) == &anotherNode);
}

static void PriorityQueueTest_pollAndInsertFrontSingle() {
    PriorityQueue pq;
    PriorityQueue_init(&pq);
    PriorityQueueNode node = { .key = 42 };
    PriorityQueueNode anotherNode = { .key = 100 };
    PriorityQueue_insert(&pq, &node);
    
    PriorityQueueNode *polled = PriorityQueue_pollAndInsert(&pq, &anotherNode, true);

    ASSERT(polled == &node);
    ASSERT(PriorityQueue_isEmpty(&pq) == false);
    ASSERT(PriorityQueue_peek(&pq) == &anotherNode);
}

static void PriorityQueueTest_insertTwo() {
    PriorityQueue pq;
    PriorityQueue_init(&pq);
    PriorityQueueNode node = { .key = 42 };
    PriorityQueueNode anotherNode = { .key = 42 };
    
    PriorityQueue_insert(&pq, &node);
    PriorityQueue_insert(&pq, &anotherNode);

    ASSERT(PriorityQueue_poll(&pq) == &node);
    ASSERT(PriorityQueue_poll(&pq) == &anotherNode);
    ASSERT(PriorityQueue_isEmpty(&pq) == true);
}

static void PriorityQueueTest_insertFrontTwo() {
    PriorityQueue pq;
    PriorityQueue_init(&pq);
    PriorityQueueNode node = { .key = 42 };
    PriorityQueueNode anotherNode = { .key = 42 };
    
    PriorityQueue_insert(&pq, &node);
    PriorityQueue_insertFront(&pq, &anotherNode);

    ASSERT(PriorityQueue_poll(&pq) == &anotherNode);
    ASSERT(PriorityQueue_poll(&pq) == &node);
    ASSERT(PriorityQueue_isEmpty(&pq) == true);
}

static void PriorityQueueTest_pollMany() {
    PriorityQueue pq;
    PriorityQueue_init(&pq);
    PriorityQueueNode nodes[19] = {
        { .key = 241 },
        { .key = 77 },
        { .key = 229 },
        { .key = 192 },
        { .key = 63 },
        { .key = 75 },
        { .key = 96 },
        { .key = 153 },
        { .key = 111 },
        { .key = 75 },
        { .key = 103 },
        { .key = 175 },
        { .key = 255 },
        { .key = 172 },
        { .key = 4 },
        { .key = 83 },
        { .key = 133 },
        { .key = 0 },
        { .key = 60 }
    };
    for (size_t i = 0; i < 19; i++)
        PriorityQueue_insert(&pq, &nodes[i]);

    ASSERT(PriorityQueue_poll(&pq)->key == 0);
    ASSERT(PriorityQueue_poll(&pq)->key == 4);
    ASSERT(PriorityQueue_poll(&pq)->key == 60);
    ASSERT(PriorityQueue_poll(&pq)->key == 63);
    ASSERT(PriorityQueue_poll(&pq)->key == 75);
    ASSERT(PriorityQueue_poll(&pq)->key == 75);
    ASSERT(PriorityQueue_poll(&pq)->key == 77);
    ASSERT(PriorityQueue_poll(&pq)->key == 83);
    ASSERT(PriorityQueue_poll(&pq)->key == 96);
    ASSERT(PriorityQueue_poll(&pq)->key == 103);
    ASSERT(PriorityQueue_poll(&pq)->key == 111);
    ASSERT(PriorityQueue_poll(&pq)->key == 133);
    ASSERT(PriorityQueue_poll(&pq)->key == 153);
    ASSERT(PriorityQueue_poll(&pq)->key == 172);
    ASSERT(PriorityQueue_poll(&pq)->key == 175);
    ASSERT(PriorityQueue_poll(&pq)->key == 192);
    ASSERT(PriorityQueue_poll(&pq)->key == 229);
    ASSERT(PriorityQueue_poll(&pq)->key == 241);
    ASSERT(PriorityQueue_poll(&pq)->key == 255);
    ASSERT(PriorityQueue_isEmpty(&pq) == true);
}

static void PriorityQueueTest_multipleOperations() {
    PriorityQueue pq;
    PriorityQueue_init(&pq);
    PriorityQueueNode nodes[19] = {
        { .key = 241 }, // 0
        { .key =  77 }, // 1
        { .key = 229 }, // 2
        { .key = 192 }, // 3
        { .key =  63 }, // 4
        { .key =  75 }, // 5
        { .key =  96 }, // 6
        { .key = 153 }, // 7
        { .key = 111 }, // 8
        { .key =  75 }, // 9
        { .key = 103 }, // 10
        { .key = 175 }, // 11
        { .key = 255 }, // 12
        { .key = 172 }, // 13
        { .key =   4 }, // 14
        { .key =  83 }, // 15
        { .key = 133 }, // 16
        { .key =   0 }, // 17
        { .key =  60 }  // 18
    };
    for (size_t i = 0; i < 11; i++)
        PriorityQueue_insert(&pq, &nodes[i]);
    PriorityQueue_remove(&pq, &nodes[4]);
    PriorityQueue_remove(&pq, &nodes[5]);
    PriorityQueue_remove(&pq, &nodes[0]);
    PriorityQueue_remove(&pq, &nodes[10]);
    ASSERT(PriorityQueue_poll(&pq)->key == 75);
    ASSERT(PriorityQueue_poll(&pq)->key == 77);
    ASSERT(PriorityQueue_poll(&pq)->key == 96);
    ASSERT(PriorityQueue_poll(&pq)->key == 111);
    for (size_t i = 10; i < 19; i++)
        PriorityQueue_insert(&pq, &nodes[i]);
    ASSERT(PriorityQueue_poll(&pq)->key == 0);
    ASSERT(PriorityQueue_poll(&pq)->key == 4);
    ASSERT(PriorityQueue_poll(&pq)->key == 60);
    ASSERT(PriorityQueue_poll(&pq)->key == 83);
    ASSERT(PriorityQueue_poll(&pq)->key == 103);
    ASSERT(PriorityQueue_poll(&pq)->key == 133);
    ASSERT(PriorityQueue_poll(&pq)->key == 153);
    ASSERT(PriorityQueue_poll(&pq)->key == 172);
    ASSERT(PriorityQueue_poll(&pq)->key == 175);
    ASSERT(PriorityQueue_poll(&pq)->key == 192);
    ASSERT(PriorityQueue_poll(&pq)->key == 229);
    ASSERT(PriorityQueue_poll(&pq)->key == 255);
    ASSERT(PriorityQueue_isEmpty(&pq) == true);
}

static void PriorityQueueTest_removeAll() {
    PriorityQueue pq;
    PriorityQueue_init(&pq);
    PriorityQueueNode nodes[19] = {
        { .key = 241 },
        { .key = 77 },
        { .key = 229 },
        { .key = 192 },
        { .key = 63 },
        { .key = 75 },
        { .key = 96 },
        { .key = 153 },
        { .key = 111 },
        { .key = 75 },
        { .key = 103 },
        { .key = 175 },
        { .key = 255 },
        { .key = 172 },
        { .key = 4 },
        { .key = 83 },
        { .key = 133 },
        { .key = 0 },
        { .key = 60 }
    };
    for (size_t i = 0; i < 19; i++)
        PriorityQueue_insert(&pq, &nodes[i]);
    for (size_t i = 0; i < 19; i++)
        PriorityQueue_remove(&pq, &nodes[i]);
    ASSERT(PriorityQueue_isEmpty(&pq) == true);
}

void PriorityQueueTest_run() {
    RUN_TEST(PriorityQueueTest_new);
    RUN_TEST(PriorityQueueTest_insertSingle);
    RUN_TEST(PriorityQueueTest_insertFrontSingle);
    RUN_TEST(PriorityQueueTest_pollSingle);
    RUN_TEST(PriorityQueueTest_removeSingle);
    RUN_TEST(PriorityQueueTest_insertTwo);
    RUN_TEST(PriorityQueueTest_insertFrontTwo);
    RUN_TEST(PriorityQueueTest_pollAndInsertSingle);
    RUN_TEST(PriorityQueueTest_pollAndInsertFrontSingle);
    RUN_TEST(PriorityQueueTest_pollMany);
    RUN_TEST(PriorityQueueTest_multipleOperations);
    RUN_TEST(PriorityQueueTest_removeAll);
}
