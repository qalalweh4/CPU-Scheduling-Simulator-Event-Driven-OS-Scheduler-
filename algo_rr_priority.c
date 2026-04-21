/*
 * algo_rr_priority.c  —  Turki Algethami
 *
 * Contains two preemptive schedulers:
 *   1. Round Robin  — time-quantum preemption via EVT_PREEMPT
 *   2. Priority     — preempts when a higher-priority process arrives/returns
 */

#include "scheduler.h"

/* ══════════════════════════════════════════════════════
 * PART 1 — ROUND ROBIN
 *
 * Round Robin (preemptive, time-sliced).
 * Quantum is set at init time.
 * When a process uses up its quantum it is re-enqueued at the tail.
 * ══════════════════════════════════════════════════════ */

static Process *rr_queue[MAX_PROCESSES * 2]; /* oversized to handle re-enqueues */
static int      rr_head    = 0;
static int      rr_tail    = 0;
static int      rr_quantum = DEFAULT_QUANTUM;

static void rr_init(int quantum) {
    rr_quantum = quantum;
    rr_head    = 0;
    rr_tail    = 0;
}

static void rr_enqueue(Process *p) {
    /* TODO Turki: add p to the tail of rr_queue[] (circular or simple array)
     *
     * Implementation: append to rr_queue[rr_tail] and advance rr_tail.
     * Using a simple expanding index is safe because MAX_PROCESSES * 2
     * covers the maximum number of re-enqueues in any simulation run.
     */
    int capacity = MAX_PROCESSES * 2;
    if (rr_tail - rr_head < capacity)
        rr_queue[rr_tail++ % capacity] = p;
}

static Process *rr_pick_next(void) {
    /* TODO Turki:
     *   Return the head of rr_queue[].
     *   The engine will schedule a PREEMPT event after rr_quantum time units
     *   if the burst won't finish first (handled in engine.c via uses_quantum()).
     */
    int capacity = MAX_PROCESSES * 2;
    if (rr_head == rr_tail) return NULL;
    return rr_queue[rr_head++ % capacity];
}

static int rr_should_preempt(Process *running, Process *arrived) {
    (void)running; (void)arrived;
    return 0;  /* RR preempts by quantum expiry (EVT_PREEMPT), not by arrival */
}

/* TODO Turki: this is the key hook the engine uses to decide whether to
 * schedule an EVT_PREEMPT instead of EVT_CPU_DONE when dispatching.
 * Return 1 for Round Robin so the engine knows to slice the burst. */
static int rr_uses_quantum(void) { return 1; }

static void rr_reset(void) {
    rr_head = rr_tail = 0;
}

static Scheduler rr_sched = {
    .init           = rr_init,
    .enqueue        = rr_enqueue,
    .pick_next      = rr_pick_next,
    .should_preempt = rr_should_preempt,
    .uses_quantum   = rr_uses_quantum,
    .reset          = rr_reset
};

Scheduler *get_rr_scheduler(void) { return &rr_sched; }


/* ══════════════════════════════════════════════════════
 * PART 2 — PREEMPTIVE PRIORITY
 *
 * Preemptive Priority scheduling.
 * Lower priority number = higher priority (e.g. priority 1 beats priority 3).
 * When a higher-priority process arrives or returns from I/O, it immediately
 * preempts the currently running process.
 * Ties broken by arrival time, then PID.
 * ══════════════════════════════════════════════════════ */

static Process *pri_ready[MAX_PROCESSES];
static int      pri_count = 0;

static void pri_init(int quantum) {
    (void)quantum;
    pri_count = 0;
}

static void pri_enqueue(Process *p) {
    /* TODO Turki: insert p into pri_ready[]
     *
     * Implementation: unsorted append — let pick_next do the scan.
     * Keeping it unsorted is simpler and O(n) on pick is fine for
     * MAX_PROCESSES = 64.
     */
    if (pri_count < MAX_PROCESSES)
        pri_ready[pri_count++] = p;
}

static Process *pri_pick_next(void) {
    /* TODO Turki:
     *   Scan pri_ready[], find the process with the lowest priority value
     *   (= highest urgency). Remove it and return it.
     *   Return NULL if pri_count == 0.
     *
     * Implementation:
     *   1. Linear scan to find index of highest-priority process.
     *   2. Swap with last element, decrement pri_count, return chosen.
     *   Tie-break: earlier arrival time, then lower PID.
     */
    if (pri_count == 0) return NULL;

    int best = 0;
    for (int i = 1; i < pri_count; i++) {
        int i_pri   = pri_ready[i]->priority;
        int b_pri   = pri_ready[best]->priority;

        if (i_pri < b_pri) {
            best = i;
        } else if (i_pri == b_pri) {
            /* Tie-break 1: earlier arrival time */
            if (pri_ready[i]->arrival_time < pri_ready[best]->arrival_time)
                best = i;
            /* Tie-break 2: lower PID */
            else if (pri_ready[i]->arrival_time == pri_ready[best]->arrival_time &&
                     pri_ready[i]->pid < pri_ready[best]->pid)
                best = i;
        }
    }

    Process *chosen = pri_ready[best];
    /* Remove by swapping with last element */
    pri_ready[best] = pri_ready[--pri_count];
    return chosen;
}

static int pri_should_preempt(Process *running, Process *arrived) {
    /* TODO Turki:
     *   Return 1 if arrived->priority < running->priority
     *   (arrived is more urgent, lower number = higher priority).
     *   The engine calls this on every EVT_ARRIVE and EVT_IO_DONE.
     */
    return arrived->priority < running->priority;
}

static int pri_uses_quantum(void) { return 0; }  /* no time-slice; only arrival preemption */

static void pri_reset(void) { pri_count = 0; }

static Scheduler pri_sched = {
    .init           = pri_init,
    .enqueue        = pri_enqueue,
    .pick_next      = pri_pick_next,
    .should_preempt = pri_should_preempt,
    .uses_quantum   = pri_uses_quantum,
    .reset          = pri_reset
};

Scheduler *get_priority_scheduler(void) { return &pri_sched; }
