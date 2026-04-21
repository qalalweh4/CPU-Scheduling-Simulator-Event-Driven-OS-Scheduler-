/*
 * event_queue.c  —  Abdullah AlQalalweh
 *
 * Priority queue of events sorted by time (min-heap via sorted linked list).
 * For small simulations a sorted linked list is fine; swap with a heap later.
 */

#include "scheduler.h"

Event *eq_create(int time, EventType type, int pid) {
    Event *e = malloc(sizeof(Event));
    if (!e) { perror("malloc"); exit(1); }
    e->time = time;
    e->type = type;
    e->pid  = pid;
    e->next = NULL;
    return e;
}

/* Insert in ascending time order.
 * Tie-breaking order: ARRIVE < IO_DONE < CPU_DONE < PREEMPT
 * so that arrivals at the same tick are processed before completions. */
static int event_priority(EventType t) {
    switch (t) {
        case EVT_ARRIVE:   return 0;
        case EVT_IO_DONE:  return 1;
        case EVT_CPU_DONE: return 2;
        case EVT_PREEMPT:  return 3;
        default:           return 4;
    }
}

void eq_insert(Event **head, Event *e) {
    /* Find insertion point: sort by time, then by event type priority */
    Event **cur = head;
    while (*cur) {
        int time_cmp = (*cur)->time - e->time;
        if (time_cmp > 0) break;  /* *cur is later → insert before it */
        if (time_cmp == 0 && event_priority((*cur)->type) > event_priority(e->type))
            break;  /* same time but lower priority type → insert before it */
        cur = &(*cur)->next;
    }
    e->next = *cur;
    *cur    = e;
}

/* Pop the earliest event (caller must free) */
Event *eq_pop(Event **head) {
    if (!*head) return NULL;
    Event *e = *head;
    *head = e->next;
    e->next = NULL;
    return e;
}

void eq_free(Event **head) {
    while (*head) {
        Event *tmp = *head;
        *head = (*head)->next;
        free(tmp);
    }
}
