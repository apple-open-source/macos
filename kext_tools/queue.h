#ifndef __QUEUE_H__
#define __QUEUE_H__

/*
 *    A generic doubly-linked list (queue).
 */

struct queue_entry {
    struct queue_entry *next;
    struct queue_entry *prev;
};

typedef struct queue_entry    *queue_t;
typedef struct queue_entry    queue_head_t;
typedef struct queue_entry    queue_chain_t;
typedef struct queue_entry    *queue_entry_t;

#define  queue_init(q)     ((q)->next = (q)->prev = q)

#define  queue_first(q)    ((q)->next)

#define  queue_next(qc)    ((qc)->next)

#define  queue_end(q, qe)  ((q) == (qe))

#define  queue_empty(q)    queue_end((q), queue_first(q))

/*
 *    Macro:        queue_enter
 *    Header:
 *        void queue_enter(q, elt, type, field)
 *            queue_t q;
 *            <type> elt;
 *            <type> is what's in our queue
 *            <field> is the chain field in (*<type>)
 */
#define queue_enter(head, elt, type, field)   \
do {                                          \
    if (queue_empty((head))) {                \
        (head)->next = (queue_entry_t) elt;   \
        (head)->prev = (queue_entry_t) elt;   \
        (elt)->field.next = head;             \
        (elt)->field.prev = head;             \
    } else {                                  \
        register queue_entry_t prev;          \
                                              \
        prev = (head)->prev;                  \
        (elt)->field.prev = prev;             \
        (elt)->field.next = head;             \
        (head)->prev = (queue_entry_t)(elt);  \
        ((type)prev)->field.next = (queue_entry_t)(elt); \
    }                                         \
} while(0)

/*
 *    Macro:        queue_field [internal use only]
 *    Function:
 *        Find the queue_chain_t (or queue_t) for the
 *        given element (thing) in the given queue (head)
 */
#define    queue_field(head, thing, type, field)  \
    (((head) == (thing)) ? (head) : &((type)(thing))->field)

/*
 *    Macro:        queue_remove
 *    Header:
 *        void queue_remove(q, qe, type, field)
 *            arguments as in queue_enter
 */
#define    queue_remove(head, elt, type, field)           \
do {                                                      \
    register queue_entry_t    next, prev;                 \
                                                          \
    next = (elt)->field.next;                             \
    prev = (elt)->field.prev;                             \
                                                          \
    queue_field((head), next, type, field)->prev = prev;  \
    queue_field((head), prev, type, field)->next = next;  \
} while(0)

#endif __QUEUE_H__
