/**
 * @authors
 * Odin Bjerke <odin.bjerke@uit.no>
 *
 * @implements set.h
 *
 * @brief Set implementation using red-black binary search tree, in-order morris iterator.
 *
 * For more info, see:
 * Red Black Tree Properties: https://en.wikipedia.org/wiki/Red%E2%80%93black_tree#Properties
 * Morris Traversal: https://en.wikipedia.org/wiki/Tree_traversal#Morris_in-order_traversal_using_threading
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "printing.h"
#include "defs.h"
#include "common.h"
#include "list.h"
#include "set.h"

typedef enum tnode_color {
    RED = 0,
    BLACK,
} tnode_color_t;

typedef struct tnode tnode_t;
struct tnode {
    tnode_color_t color;
    void *elem;
    tnode_t *parent;
    tnode_t *left;
    tnode_t *right;
};

struct set {
    tnode_t *root;
    cmp_fn cmpfn;
    size_t length;
};

static tnode_t sentinel = {.color = BLACK};

/**
 * The sentinel-node (NIL) functions as a 'colored NULL-pointer' for leaf nodes.
 * Eliminates a lot of edge-case conditions for the various rotations, etc.
 */
#define NIL &sentinel


/* ------------------Runtime validation------------------ */

static void rec_validate_rbtree(tnode_t *node, int black_count, int *path_black_count) {
    /* prop 4: every path from a given node to any of its descendant NIL nodes goes through the same number of
     * black nodes */
    if (node == NIL) {
        if (*path_black_count == -1) {
            *path_black_count = black_count;
        }
        assertf(
            (black_count == *path_black_count),
            "expected count of %d, found %d\n",
            black_count,
            *path_black_count
        );
        return;
    }

    /* prop 3: a red node does not have a red child */
    if (node->color == RED) {
        assert(node != NIL);
        assert(node->left->color != RED && node->right->color != RED);
    } else {
        black_count++; // update black count to track at leaf level
    }

    /* Recursively verify left and right subtrees */
    rec_validate_rbtree(node->left, black_count, path_black_count);
    rec_validate_rbtree(node->right, black_count, path_black_count);
}

/**
 * For debugging. Verifies that the tree is in fact balanced. Do not use during iteration.
 */
ATTR_MAYBE_UNUSED
static void validate_rbtree(set_t *set) {
    if (set->root == NIL) {
        return;
    }

    /* Property 1: Root must be black */
    assert(set->root->color == BLACK);

    int path_black_count = -1;
    rec_validate_rbtree(set->root, 0, &path_black_count);
}

/* --------------------Create, Destroy-------------------- */

set_t *set_create(cmp_fn cmpfn) {
    set_t *set = malloc(sizeof(set_t));
    if (set == NULL) {
        pr_error("Malloc failed @set_create\n");
        return NULL;
    }

    set->root = NIL;
    set->cmpfn = cmpfn;
    set->length = 0;

    return set;
}

size_t set_length(set_t *set) {
    return set->length;
}

/**
 * @brief Recursive part of set_destroy. Probably some neat way to this without the overhead of recursion,
 * but whatever.
 */
static void rec_postorder_destroy(set_t *set, tnode_t *node, free_fn elem_freefn) {
    if (node == NIL) {
        return;
    }

    rec_postorder_destroy(set, node->left, elem_freefn);
    rec_postorder_destroy(set, node->right, elem_freefn);

    /**
     * Free self when returned from the recursive call stack. At this point, any children are also freed.
     * This results in the entire tree being freed in a bottom-up (postorder) manner, so that no nodes depend
     * on freed nodes for traversal a freed node depend on this node for traversal free self when returned
     */
    if (elem_freefn) {
        elem_freefn(node->elem);
    }
    free(node);
}

void set_destroy(set_t *set, free_fn elem_freefn) {
    if (!set) {
        return;
    }
    rec_postorder_destroy(set, set->root, elem_freefn);
    free(set);
}

/* ------------------------Rotation----------------------- */

/* rotate node counter-clockwise */
static inline void rotate_left(set_t *set, tnode_t *u) {
    tnode_t *v = u->right;

    u->right = v->left;
    if (v->left != NIL) {
        v->left->parent = u;
    }

    v->parent = u->parent;
    if (u->parent == NIL) {
        set->root = v;
    } else if (u == u->parent->left) {
        u->parent->left = v;
    } else {
        u->parent->right = v;
    }

    v->left = u;
    u->parent = v;
}

/* rotate node clockwise */
static inline void rotate_right(set_t *set, tnode_t *u) {
    tnode_t *v = u->left;

    u->left = v->right;
    if (v->right != NIL) {
        v->right->parent = u;
    }

    v->parent = u->parent;
    if (u->parent == NIL) {
        set->root = v;
    } else if (u == u->parent->right) {
        u->parent->right = v;
    } else {
        u->parent->left = v;
    }

    v->right = u;
    u->parent = v;
}

/* -----------------------Insertion----------------------- */

/* balance the tree (set) after adding a node */
static inline void post_insert_balance(set_t *set, tnode_t *added_node) {
    tnode_t *curr = added_node;

    while (curr->parent->color == RED) {
        tnode_t *par = curr->parent; // parent
        tnode_t *gp = par->parent;   // grandparent

        bool par_is_leftchild = (gp->left == par);
        tnode_t *unc = par_is_leftchild ? gp->right : gp->left; // uncle

        if (unc->color == RED) {
            /* case 1: red uncle - recolor and move up the tree */
            unc->color = BLACK;
            par->color = BLACK;
            gp->color = RED;
            curr = gp;
        } else {
            /* Case 2 & 3: black uncle - rotation needed */
            if (par_is_leftchild) {
                if (curr == par->right) {
                    /* Case 2a: Left-Right */
                    rotate_left(set, par);
                    curr = par;
                    par = curr->parent;
                }
                /* case 3a: Left-Left */
                rotate_right(set, gp);
            } else {
                if (curr == par->left) {
                    /* case 2b: Right-Left */
                    rotate_right(set, par);
                    curr = par;
                    par = curr->parent;
                }
                /* case 3b: Right-Right se */
                rotate_left(set, gp);
            }

            /* fix colors after rotation */
            par->color = BLACK;
            gp->color = RED;
            break;
        }
    }

    // ensure the root is always black
    set->root->color = BLACK;
}

void *set_insert(set_t *set, void *elem) {
    if (set->root == NIL) {
        set->root = malloc(sizeof(tnode_t));
        if (!set->root) {
            PANIC("Out of memory\n");
        }

        /* only time we insert a black node */
        set->root->color = BLACK;
        set->root->elem = elem;
        set->root->left = set->root->right = set->root->parent = NIL;
        set->length += 1;

        return NULL;
    }

    tnode_t *curr = set->root;
    int cmp;

    /* traverse until we find a suitable parent or equal elem */
    while (1) {
        cmp = set->cmpfn(elem, curr->elem);

        if (cmp > 0) {
            if (curr->right == NIL) {
                break;
            }
            // a < b => move right in tree
            curr = curr->right;
        } else if (cmp < 0) {
            // a > b => move left in tree
            if (curr->left == NIL) {
                break;
            }
            // else ... move left in tree
            curr = curr->left;
        } else {
            /* swap elems */
            void *replaced = curr->elem;
            curr->elem = elem;

            return replaced;
        }
    }

    tnode_t *node = malloc(sizeof(tnode_t));
    if (!node) {
        PANIC("Out of memory\n");
    }

    node->color = RED;
    node->elem = elem;
    node->left = node->right = NIL;
    node->parent = curr;

    if (cmp > 0) {
        curr->right = node;
    } else {
        curr->left = node;
    }

    set->length += 1;
    post_insert_balance(set, node);

    validate_rbtree(set); // perform extensive validation after each insertion

    return NULL;
}

/* -------------------------Search------------------------ */

static inline tnode_t *node_search(set_t *set, void *elem) {
    tnode_t *curr = set->root;

    /* traverse until a NIL-node, or return is an equal element is found */
    while (curr != NIL) {
        int direction = set->cmpfn(elem, curr->elem);

        if (direction > 0) {
            /* a < b    => curr < target   => go right */
            curr = curr->right;
        } else if (direction < 0) {
            /* a > b    => curr > target   => go left */
            curr = curr->left;
        } else {
            /* current node holds the target element */
            break;
        }
    }

    /* NIL if we reached the end, otherwise the target */
    return curr;
}

void *set_get(set_t *set, void *elem) {
    tnode_t *node = node_search(set, elem);

    return node->elem;
}

/* ---------------------Set operations-------------------- */

/**
 * Recursive part of set_copy. Highly optimized.
 * Copies each node with no comparisons.
 */
static tnode_t *rec_set_copy(tnode_t *orig_node, tnode_t *parent) {
    if (orig_node == NIL) {
        return NIL;
    }

    tnode_t *new_node = malloc(sizeof(tnode_t));
    if (!new_node) {
        PANIC("Failed to allocate memory during set copy\n");
    }

    new_node->color = orig_node->color;
    new_node->elem = orig_node->elem;
    new_node->parent = parent;

    new_node->left = rec_set_copy(orig_node->left, new_node);
    new_node->right = rec_set_copy(orig_node->right, new_node);

    return new_node;
}

static set_t *set_copy(set_t *set) {
    set_t *set_cpy = set_create(set->cmpfn);
    if (!set_cpy) {
        return NULL;
    }

    set_cpy->length = set->length;
    set_cpy->root = rec_set_copy(set->root, NIL);

    return set_cpy;
}

/**
 * Recursive part of set_copy.
 * Copies each node with no comparisons.
 */
static void rec_set_merge(set_t *target, tnode_t *root) {
    if (root == NIL) {
        return;
    }

    rec_set_merge(target, root->right);
    rec_set_merge(target, root->left);
    set_insert(target, root->elem);
}

set_t *set_union(set_t *a, set_t *b) {
    /**
     * pick the smallest set to copy, given that they have the same compare function, otherwise we
     * have to stick with 'a' since since we are about to create a literal shallow copy of the set
     */
    if (a->length < b->length && a->cmpfn == b->cmpfn) {
        set_t *tmp = a;
        a = b;
        b = tmp;
    }

    set_t *c = set_copy(a);
    if (!c) {
        pr_error("Not enough memory to perform set union\n");
        return NULL;
    }

    /* if a is b, c == a || b, so no point in merging. return copy of a. */
    if (a != b) {
        rec_set_merge(c, b->root);
    }

    return c;
}

/**
 * Recursive part of set_intersection
 */
static void rec_set_intersection(set_t *c, set_t *b, tnode_t *root_a) {
    if (root_a == NIL) {
        return;
    }

    rec_set_intersection(c, b, root_a->left);
    rec_set_intersection(c, b, root_a->right);

    /* post order recursion here prevents items from being added in the worst-case fashion if sorted */
    if (set_get(b, root_a->elem)) {
        set_insert(c, root_a->elem);
    }
}

set_t *set_intersection(set_t *a, set_t *b) {
    /* if a is b, c == a || b, so simply copy 'a' */
    if (a == b) {
        return set_copy(a);
    }

    set_t *c = set_create(a->cmpfn);
    if (!c) {
        return NULL;
    }

    /* pick the longest set as lead, as it might lead to drastically faster searches */
    if (a->length < b->length) {
        set_t *tmp = a;
        a = b;
        b = tmp;
    }

    rec_set_intersection(c, b, a->root);

    return c;
}

static void rec_set_difference(set_t *c, set_t *b, tnode_t *root_a) {
    if (root_a == NIL) {
        return;
    }

    rec_set_difference(c, b, root_a->left);
    rec_set_difference(c, b, root_a->right);

    /* post order recursion here prevents items from being added in the worst-case fashion if sorted */
    if (set_get(b, root_a->elem) == NULL) {
        set_insert(c, root_a->elem);
    }
}

set_t *set_difference(set_t *a, set_t *b) {
    /**
     * This one is more tricky to optimize, as (a - b) != (b - a).
     * However, we do want to do it recursively, as our iterator is in-order,
     * which would lead to the worst-case insertion pattern.
     */

    set_t *c = set_create(a->cmpfn);
    if (!c) {
        return NULL;
    }

    /* if a is b, c == { Ø }, so no point in merging. Return empty set. */
    if (a != b) {
        rec_set_difference(c, b, a->root);
    }

    return c;
}


/* -----------------------Iteration----------------------- */

typedef struct set_iter {
    set_t *set;
    tnode_t *head;
} set_iter_t;

/**
 * @brief In-order morris traversal implementation.
 *
 * Temporarily alters the tree structure to form a in-order sequence, using the freely available pointers at
 * leaf nodes.
 *
 * @note
 * This implementation is extremely efficient, but makes insertions to the tree during active iteration
 * potentially catastrophic (although unlikely for a large set, as it only affects one node at any given
 * point). However; As a safety measure, I attached a check for this at the insertion stage,
 * where a warning is printed.
 */
static tnode_t *next_node_inorder(set_iter_t *iter) {
    tnode_t *next = iter->head;

    while (next != NIL) {
        if (next->left == NIL) {
            /* no left subtree, visit current node and move right */
            iter->head = next->right;
            return next;
        }

        /* find the in-order predecessor */
        tnode_t *pre = next->left;
        while (pre->right != NIL && pre->right != next) {
            pre = pre->right;
        }

        if (pre->right == NIL) {
            /* create temporary link to return after left subtree traversal */
            pre->right = next;
            next = next->left;
        } else {
            /* restore tree structure, visit current node, move right */
            pre->right = NIL;
            iter->head = next->right;
            return next;
        }
    }

    return NIL;
}

set_iter_t *set_createiter(set_t *set) {
    if (!set) {
        PANIC("Attempt to create iterator for set=NULL\n");
    }

    set_iter_t *iter = malloc(sizeof(set_iter_t));
    if (iter == NULL) {
        pr_error("Failed to allocate memory\n");
        return NULL;
    }

    iter->set = set;
    iter->head = set->root;

    return iter;
}

int set_hasnext(set_iter_t *iter) {
    return (iter->head == NIL) ? 0 : 1;
}

void set_destroyiter(set_iter_t *iter) {
    while (set_hasnext(iter)) {
        // finish the morris iterator process to avoid leaving any mutated leaves
        next_node_inorder(iter);
    }

    free(iter);
}

void *set_next(set_iter_t *iter) {
    tnode_t *curr = next_node_inorder(iter);
    /* if end of tree is reached, curr->elem will be NULL */
    return curr->elem;
}
