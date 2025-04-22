/**
 * @authors
 * Steffen Viken Valvaag <steffenv@cs.uit.no>
 * Odin Bjerke <odin.bjerke@uit.no>
 *
 * @implements list.h
 *
 * @brief doubly linked list implementation with merge sort
 */

#include <stdlib.h>

#include "printing.h"
#include "defs.h"
#include "list.h"


typedef struct lnode lnode_t;
struct lnode {
    lnode_t *right;
    lnode_t *left;
    void *item;
};

struct list {
    lnode_t *leftmost;
    lnode_t *rightmost;
    size_t length;
    cmp_fn cmpfn;
};

struct list_iter {
    list_t *list;
    lnode_t *node;
};


static lnode_t *newnode(void *item) {
    lnode_t *node = malloc(sizeof(lnode_t));
    if (!node) {
        pr_error("Cannot allocate memory\n");
        return NULL;
    }
    node->right = NULL;
    node->left = NULL;
    node->item = item;

    return node;
}

list_t *list_create(cmp_fn cmpfn) {
    list_t *list = malloc(sizeof(list_t));
    if (!list) {
        pr_error("Cannot allocate memory\n");
        return NULL;
    }

    list->leftmost = NULL;
    list->rightmost = NULL;
    list->length = 0;
    list->cmpfn = cmpfn;

    return list;
}

void list_destroy(list_t *list, free_fn item_free) {
    if (!list) {
        return;
    }
    while (list->leftmost != NULL) {
        lnode_t *right = list->leftmost->right;
        if (item_free) {
            item_free(list->leftmost->item);
        }
        free(list->leftmost);
        list->leftmost = right;
    }

    free(list);
}

size_t list_length(list_t *list) {
    return list->length;
}

int list_addfirst(list_t *list, void *item) {
    lnode_t *node = newnode(item);
    if (node == NULL) {
        return -1;
    }

    if (list->leftmost == NULL) {
        list->leftmost = list->rightmost = node;
    } else {
        list->leftmost->left = node;
        node->right = list->leftmost;
        list->leftmost = node;
    }
    list->length++;

    return 0;
}

int list_addlast(list_t *list, void *item) {
    lnode_t *node = newnode(item);
    if (node == NULL) {
        return -1;
    }

    if (list->leftmost == NULL) {
        list->leftmost = list->rightmost = node;
    } else {
        list->rightmost->right = node;
        node->left = list->rightmost;
        list->rightmost = node;
    }
    list->length++;

    return 0;
}

void *list_popfirst(list_t *list) {
    if (!list->length) {
        PANIC("attempt to pop first from empty list\n");
    }

    lnode_t *tmp = list->leftmost;
    void *item = tmp->item;

    list->leftmost = list->leftmost->right;

    if (list->leftmost == NULL) {
        list->rightmost = NULL;
    } else {
        assert(list->leftmost);
        list->leftmost->left = NULL;
    }

    list->length--;
    free(tmp);

    return item;
}

void *list_poplast(list_t *list) {
    if (!list->length) {
        PANIC("attempt to pop last from empty list\n");
    }

    lnode_t *tmp = list->rightmost;
    void *item = tmp->item;

    list->rightmost = list->rightmost->left;

    if (list->rightmost == NULL) {
        list->leftmost = NULL;
    } else {
        list->rightmost->right = NULL;
    }

    list->length--;
    free(tmp);

    return item;
}

void *list_remove(list_t *list, void *item) {
    lnode_t *node = list->leftmost;

    while (node != NULL) {
        if (list->cmpfn(item, node->item) == 0) {
            break;
        }
        node = node->right;
    }

    if (!node) {
        /* not found */
        return NULL;
    }

    void *found = node->item;

    /* splice the node from the list */

    if (list->leftmost == node) {
        list->leftmost = node->right;
    } else {
        /* only node without a left is leftmost, so there is a node->left */
        node->left->right = node->right;
    }

    if (list->rightmost == node) {
        list->rightmost = node->left;
    } else {
        /* only node without a right is rightmost, so there is a node->right  */
        node->right->left = node->left;
    }

    free(node);

    return found;
}

int list_contains(list_t *list, void *item) {
    lnode_t *node = list->leftmost;

    while (node != NULL) {
        if (list->cmpfn(item, node->item) == 0) {
            return 1;
        }
        node = node->right;
    }

    return 0;
}

/*
 * Merges two sorted lists a and b using the given comparison function.
 * Only assigns the right pointers; the left pointers will have to be
 * fixed by the caller.  Returns the leftmost of the merged list.
 */
static lnode_t *merge(lnode_t *a, lnode_t *b, cmp_fn cmpfn) {
    lnode_t *leftmost, *rightmost;

    /* Pick the smallest leftmost node */
    if (cmpfn(a->item, b->item) < 0) {
        leftmost = rightmost = a;
        a = a->right;
    } else {
        leftmost = rightmost = b;
        b = b->right;
    }

    /* Now repeatedly pick the smallest leftmost node */
    while (a && b) {
        if (cmpfn(a->item, b->item) < 0) {
            rightmost->right = a;
            rightmost = a;
            a = a->right;
        } else {
            rightmost->right = b;
            rightmost = b;
            b = b->right;
        }
    }

    /* Append the remaining non-empty list (if any) */
    if (a) {
        rightmost->right = a;
    } else {
        rightmost->right = b;
    }

    return leftmost;
}

/**
 * Splits the given list in two halves, and returns the leftmost of
 * the second half.
 */
static lnode_t *splitlist(lnode_t *leftmost) {
    /* Move two pointers, a 'slow' one and a 'fast' one which moves
     * twice as fast.  When the fast one reaches the end of the list,
     * the slow one will be at the middle.
     */
    lnode_t *slow = leftmost;
    lnode_t *fast = leftmost->right;

    while (fast != NULL && fast->right != NULL) {
        slow = slow->right;
        fast = fast->right->right;
    }

    /* Now 'cut' the list and return the second half */
    lnode_t *half = slow->right;
    slow->right = NULL;

    return half;
}

/**
 * Recursive merge sort.  This function is named mergesort_ to avoid
 * collision with the mergesort function that is defined by the standard
 * library on some platforms.
 */
static lnode_t *mergesort_(lnode_t *leftmost, cmp_fn cmpfn) {
    if (leftmost->right == NULL) {
        return leftmost;
    }

    lnode_t *half = splitlist(leftmost);
    leftmost = mergesort_(leftmost, cmpfn);
    half = mergesort_(half, cmpfn);

    return merge(leftmost, half, cmpfn);
}

void list_sort(list_t *list) {
    if (list->length < 2) {
        return;
    }

    /* Recursively sort the list */
    list->leftmost = mergesort_(list->leftmost, list->cmpfn);

    /* Fix the rightmost and left links */
    lnode_t *left = NULL;
    for (lnode_t *n = list->leftmost; n != NULL; n = n->right) {
        n->left = left;
        left = n;
    }
    list->rightmost = left;
}

list_iter_t *list_createiter(list_t *list) {
    list_iter_t *iter = malloc(sizeof(list_iter_t));
    if (iter == NULL) {
        pr_error("Cannot allocate memory\n");
        return NULL;
    }

    iter->list = list;
    iter->node = list->leftmost;

    return iter;
}

void list_destroyiter(list_iter_t *iter) {
    free(iter);
}

int list_hasnext(list_iter_t *iter) {
    return (iter->node == NULL) ? 0 : 1;
}

void *list_next(list_iter_t *iter) {
    if (iter->node == NULL) {
        return NULL;
    }

    void *item = iter->node->item;
    iter->node = iter->node->right;

    return item;
}

void list_resetiter(list_iter_t *iter) {
    iter->node = iter->list->leftmost;
}
