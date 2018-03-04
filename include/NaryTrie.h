/*
Generic intrusive n-ary bitwise trie container.
Copyright 2012-2018 Salvatore ISAJA. All rights reserved.

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

/******************************************************************************
 * This is a poor man's template file.
 * In order to use the container, you need to instantiate the poor man's
 * template macros for header and implementation.
 ******************************************************************************/
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * Instantiates the header for an intrusive n-ary trie.
 * @param Trie name of the container to instantiate.
 * @param logChildCount base 2 logarithm of the number of children for each node.
 * @param Key unsigned integral type for node keys.
 */
#define Trie_header(Trie, logChildCount, Key) \
\
typedef struct Trie Trie;\
typedef struct Trie##_Node Trie##_Node;\
\
struct Trie##_Node {\
    Trie##_Node *parent;\
    Trie##_Node *children[(1 << logChildCount)];\
    Trie##_Node *prev;\
    Trie##_Node *next;\
};\
\
struct Trie {\
    Trie##_Node *root;\
    size_t keyBits;\
};\
\
/** Initializes a trie specifying the number of significant bits in its keys. */\
static inline void Trie##_initialize(Trie *trie, size_t keyBits) {\
    trie->root = NULL;\
    trie->keyBits = keyBits;\
}\
\
static inline bool Trie##_isEmpty(const Trie *trie) {\
    return trie->root == NULL;\
}\
\
void         Trie##_insert(Trie *trie, Trie##_Node *newNode, bool prepend);\
void         Trie##_remove(Trie *trie, Trie##_Node *node);\
Trie##_Node *Trie##_find(const Trie *trie, Key key);\
Trie##_Node *Trie##_findMin(const Trie *trie);\
Trie##_Node *Trie##_findMax(const Trie *trie);\
Trie##_Node *Trie##_findEqualOrLarger(const Trie *trie, Key key);\
void         Trie##_check(const Trie *trie);


/**
 * Instantiates the implementation for an intrusive n-ary trie.
 * @param Trie name of the container to instantiate.
 * @param logChildCount base 2 logarithm of the number of children for each node.
 * @param Key unsigned integral type for node keys.
 * @param getKey function taking a pointer to a node and returning its key.
 */
#define Trie_implementation(Trie, logChildCount, Key, getKey) \
\
static void updateChild(Trie##_Node *node, Trie##_Node *oldChild, Trie##_Node *newChild) {\
    for (size_t i = 0; i < (1 << logChildCount); i++) {\
        if (node->children[i] == oldChild) {\
            node->children[i] = newChild;\
            return;\
        }\
    }\
    assert(false);\
}\
\
static Trie##_Node *findLeftmostChild(const Trie##_Node *node) {\
    for (size_t i = 0; i < (1 << logChildCount) - 1; i++) {\
        if (node->children[i] != NULL) return node->children[i];\
    }\
    return node->children[(1 << logChildCount) - 1];\
}\
\
static Trie##_Node *findRightmostChild(const Trie##_Node *node) {\
    for (size_t i = (1 << logChildCount) - 1; i > 0; i--) {\
        if (node->children[i] != NULL) return node->children[i];\
    }\
    return node->children[0];\
}\
\
static bool hasChildren(const Trie##_Node *node) {\
    for (size_t i = 0; i < (1 << logChildCount); i++) {\
        if (node->children[i] != NULL) return true;\
    }\
    return false;\
}\
\
static void replaceNode(Trie *trie, Trie##_Node *oldNode, Trie##_Node *newNode) {\
    if (oldNode->parent != NULL) {\
        updateChild(oldNode->parent, oldNode, newNode);\
    }\
    if (newNode != NULL) {\
        newNode->parent = oldNode->parent;\
        for (size_t i = 0; i < (1 << logChildCount); i++) {\
            newNode->children[i] = oldNode->children[i];\
            if (newNode->children[i] != NULL) newNode->children[i]->parent = newNode;\
        }\
    }\
    if (trie->root == oldNode) trie->root = newNode;\
}\
\
/**
 * Inserts the specified node into the trie, before of after nodes sharing
 * the same key, depending on the "prepend" parameter.
 */\
void Trie##_insert(Trie *trie, Trie##_Node *newNode, bool prepend) {\
    for (size_t i = 0; i < (1 << logChildCount); i++) newNode->children[i] = NULL;\
    newNode->prev = newNode;\
    newNode->next = newNode;\
    if (trie->root == NULL) {\
        trie->root = newNode;\
        newNode->parent = NULL;\
        return;\
    }\
    Key newKey = getKey(newNode);\
    Trie##_Node *current = trie->root;\
    for (int bitShift = trie->keyBits - logChildCount; ; bitShift -= logChildCount) {\
        Key currentKey = getKey(current);\
        if (currentKey == newKey) {\
            /* Nodes with the same key are stored in a circular list.
             * The first node joins the tree, whereas the other ones
             * have the parent link set to themselves as identification,
             * and the children links are not meaningful.
             */\
            newNode->prev = current->prev;\
            newNode->next = current;\
            current->prev->next = newNode;\
            current->prev = newNode;\
            newNode->parent = newNode;\
            if (prepend) {\
                replaceNode(trie, current, newNode);\
                current->parent = current;\
            }\
            return;\
        }\
        assert(bitShift >= 0); /* eventually we must find a node to append the new value to */\
        size_t childIndex = (newKey >> bitShift) & (Key) ((1 << logChildCount) - 1);\
        Trie##_Node *child = current->children[childIndex];\
        if (child == NULL) {\
            current->children[childIndex] = newNode;\
            newNode->parent = current;\
            return;\
        }\
        current = child;\
    }\
}\
\
/** Removes the specified node from the trie. */\
void Trie##_remove(Trie *trie, Trie##_Node *node) {\
    /* First check if the node to be removed can be replaced by a sibling */\
    if (node->next != node) {\
        /* The node to be removed is part of a circular list of nodes sharing the same key */\
        assert(node->next != NULL);\
        node->next->prev = node->prev;\
        node->prev->next = node->next;\
        if (node->parent != node) {\
            /* The node to be removed is the first element of the list,
             * that is the one which joins the tree. */\
            replaceNode(trie, node, node->next);\
        }\
        if (trie->root == node) trie->root = node->next;\
        return;\
    }\
    /* If the node has no children and no siblings, we can just remove it */\
    if (!hasChildren(node)) {\
        assert(node->next == node);\
        if (node->parent != NULL) {\
            assert(trie->root != node);\
            updateChild(node->parent, node, NULL);\
        } else {\
            assert(trie->root == node);\
            trie->root = NULL;\
        }\
        return;\
    }\
    /* If the node has children, replace it with a leaf */\
    for (Trie##_Node *current = findLeftmostChild(node); ; ) {\
        Trie##_Node *leftmostChild = findLeftmostChild(current);\
        if (leftmostChild == NULL) {\
            assert(current->parent != NULL);\
            updateChild(current->parent, current, NULL);\
            replaceNode(trie, node, current);\
            return;\
        }\
        current = leftmostChild;\
    }\
}\
\
/**
 * Finds the node with the specified key, if any.
 * Returns NULL if not found.
 */\
Trie##_Node *Trie##_find(const Trie *trie, Key key) {\
    Trie##_Node *current = trie->root;\
    for (int bitShift = trie->keyBits - logChildCount; (bitShift >= 0) && (current != NULL); bitShift -= logChildCount) {\
        if (getKey(current) == key) break;\
        size_t childIndex = (key >> bitShift) & (Key) ((1 << logChildCount) - 1);\
        current = current->children[childIndex];\
    }\
    return current;\
}\
\
/** Finds the node with the smallest key. */\
Trie##_Node *Trie##_findMin(const Trie *trie) {\
    Trie##_Node *current = trie->root;\
    Key minKey = getKey(current);\
    Trie##_Node *minNode = current;\
    while (true) {\
        current = findLeftmostChild(current);\
        if (current == NULL) break;\
        Key currentKey = getKey(current);\
        if (currentKey < minKey) {\
            minKey = currentKey;\
            minNode = current;\
        }\
    }\
    return minNode;\
}\
\
/** Finds the node with the largest key. */\
Trie##_Node *Trie##_findMax(const Trie *trie) {\
    Trie##_Node *current = trie->root;\
    Key maxKey = getKey(current);\
    Trie##_Node *maxNode = current;\
    while (true) {\
        current = findRightmostChild(current);\
        if (current == NULL) break;\
        Key currentKey = getKey(current);\
        if (currentKey > maxKey) {\
            maxKey = currentKey;\
            maxNode = current;\
        }\
    }\
    return maxNode;\
}\
\
/**
 * Finds the node with the least key equal to or larget than the specified key.
 * Returns NULL if not found.
 */\
Trie##_Node *Trie##_findEqualOrLarger(const Trie *trie, Key key) {\
    Trie##_Node *current = trie->root;\
    Key bestMatchKey;\
    Trie##_Node *bestMatch = NULL;\
    for (int bitShift = trie->keyBits - logChildCount; (bitShift >= 0) && (current != NULL); bitShift -= logChildCount) {\
        Key currentKey = getKey(current);\
        if (currentKey == key) {\
            return current; /* Exact match */\
        }\
        if (bestMatch == NULL || (currentKey > key && currentKey < bestMatchKey)) {\
            bestMatchKey = currentKey;\
            bestMatch = current;\
        }\
        size_t childIndex = (key >> bitShift) & (Key) ((1 << logChildCount) - 1);\
        for (size_t i = childIndex + 1; i < (1 << logChildCount); i++) {\
            Trie##_Node *child = current->children[i];\
            if (child != NULL) {\
                Key childKey = getKey(child);\
                if (childKey > key && childKey < bestMatchKey) {\
                    bestMatchKey = childKey;\
                    bestMatch = child;\
                    break;\
                }\
            }\
        }\
        current = current->children[childIndex];\
    }\
    /* If we arrive here, we have a best match that may be larger than the
     * smaller key larger than the key been searched. The current best match
     * is either the root or a node with key larger than the one being searched.
     */\
    if (bestMatch != NULL) {\
        Trie##_Node *current = bestMatch;\
        while (true) {\
            current = findLeftmostChild(current);\
            if (current == NULL) break;\
            Key currentKey = getKey(current);\
            if (currentKey > key && currentKey < bestMatchKey) {\
                bestMatchKey = currentKey;\
                bestMatch = current;\
            }\
        }\
    }\
    return bestMatch;\
}


/**
 * Instantiates the implementation for checking invariants of an intrusive n-ary trie.
 * @param Trie name of the container to instantiate.
 * @param logChildCount base 2 logarithm of the number of children for each node.
 * @param Key unsigned integral type for node keys.
 * @param getKey function taking a pointer to a node and returning its key.
 */
#define Trie_debugImplementation(Trie, logChildCount, Key, getKey) \
\
static void check(const Trie *trie, Trie##_Node *node, int bitShift) {\
    if (node == NULL) return;\
    assert(bitShift >= 0);\
    assert((node->parent != NULL) || (node == trie->root));\
    for (size_t i = 0; i < (1 << logChildCount); i++) {\
        if (node->children[i] != NULL) assert(node->children[i]->parent == node);\
    }\
    for (Trie##_Node *prev = node, *curr = node->next; curr != node; prev = curr, curr = curr->next) {\
        assert(curr->parent == curr);\
        assert(curr->prev == prev);\
        assert(getKey(curr) == getKey(prev));\
    }\
    Trie##_Node *climbingNode = node;\
    for (size_t climbingBitShift = bitShift; ; climbingBitShift += logChildCount) {\
        Key climbingKey = getKey(climbingNode);\
        if (climbingNode->parent == NULL) {\
            assert(climbingNode == trie->root);\
            assert(climbingBitShift == trie->keyBits);\
            break;\
        }\
        Trie##_Node *climbingNodeParent = climbingNode->parent;\
        size_t i;\
        for (i = 0; i < (1 << logChildCount); i++) {\
            if (climbingNodeParent->children[i] == climbingNode) break;\
        }\
        assert(i < (1 << logChildCount));\
        size_t childIndex = (climbingKey >> climbingBitShift) & (Key) ((1 << logChildCount) - 1);\
        assert(i == childIndex);\
        climbingNode = climbingNode->parent;\
    }\
    for (size_t i = 0; i < (1 << logChildCount); i++) {\
        check(trie, node->children[i], bitShift - logChildCount);\
    }\
}\
\
/** Checks structural invariants for the trie. */\
void Trie##_check(const Trie *trie) {\
    check(trie, trie->root, trie->keyBits);\
}
