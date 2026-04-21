/*
 * algo_fcfs_sjf.c  —  AHMED
 *
 * Contains two non-preemptive schedulers:
 *   1. FCFS  — First-Come First-Served (FIFO order)
 *   2. SJF   — Shortest Job First (scan for smallest remaining burst)
 */

#include "scheduler.h"

/* ══════════════════════════════════════════════════════
 * PART 1 — FCFS
 *
 * First-Come First-Served (non-preemptive).
 * Ready queue = simple FIFO. No preemption ever.
 * ══════════════════════════════════════════════════════ */

/* ── Internal FIFO queue ── */
static Process *fifo[MAX_PROCESSES];
static int      fifo_head = 0, fifo_tail = 0;

static void fcfs_init(int quantum) {
    (void)quantum;   /* FCFS ignores quantum */
    fifo_head = fifo_tail = 0;
}

static void fcfs_enqueue(Process *p) {
    /* TODO Ahmed: add p to the tail of fifo[]
     *
     * Implementation: store the pointer at fifo[fifo_tail],
     * then advance fifo_tail. Wrap around if needed (circular buffer),
     * but since MAX_PROCESSES is a hard cap a simple array is fine.
     */
    if (fifo_tail < MAX_PROCESSES)
        fifo[fifo_tail++] = p;
}

static Process *fcfs_pick_next(void) {
    /* TODO Ahmed: return head of fifo[], or NULL if empty
     *
     * Implementation: if fifo_head == fifo_tail the queue is empty.
     * Otherwise return fifo[fifo_head] and advance fifo_head.
     */
    if (fifo_head == fifo_tail) return NULL;
    return fifo[fifo_head++];
}

static int fcfs_should_preempt(Process *running, Process *arrived) {
    (void)running; (void)arrived;
    return 0;   /* FCFS never preempts */
}

static int fcfs_uses_quantum(void) { return 0; }

static void fcfs_reset(void) {
    fifo_head = fifo_tail = 0;
}

static Scheduler fcfs_sched = {
    .init           = fcfs_init,
    .enqueue        = fcfs_enqueue,
    .pick_next      = fcfs_pick_next,
    .should_preempt = fcfs_should_preempt,
    .uses_quantum   = fcfs_uses_quantum,
    .reset          = fcfs_reset
};

Scheduler *get_fcfs_scheduler(void) { return &fcfs_sched; }


/* ══════════════════════════════════════════════════════
 * PART 2 — SJF
 *
 * Shortest Job First (non-preemptive).
 * Pick the ready process with the smallest remaining CPU burst.
 * Ties broken by arrival time (earlier arrival wins),
 * then by PID (lower PID wins) for determinism.
 * ══════════════════════════════════════════════════════ */

/* ── Internal unsorted array (scan on every pick) ── */
static Process *sjf_ready[MAX_PROCESSES];
static int      sjf_count = 0;

static void sjf_init(int quantum) {
    (void)quantum;
    sjf_count = 0;
}

static void sjf_enqueue(Process *p) {
    /* TODO Ahmed: add p to sjf_ready[]
     *
     * Implementation: SJF does not need to keep the array sorted.
     * Just append and let pick_next do a linear scan to find the shortest.
     */
    if (sjf_count < MAX_PROCESSES)
        sjf_ready[sjf_count++] = p;
}

static Process *sjf_pick_next(void) {
    /* TODO Ahmed:
     *   Scan sjf_ready[0..sjf_count-1], find the one with
     *   the smallest remaining_burst. Remove it and return it.
     *   Return NULL if sjf_count == 0.
     *
     * Implementation:
     *   1. Find the index of the minimum-burst process.
     *   2. Swap it with the last element to remove it in O(1).
     *   3. Decrement sjf_count and return the chosen process.
     */
    if (sjf_count == 0) return NULL;

    int best = 0;
    for (int i = 1; i < sjf_count; i++) {
        int b_burst = sjf_ready[i]->remaining_burst;
        int c_burst = sjf_ready[best]->remaining_burst;

        if (b_burst < c_burst) {
            best = i;
        } else if (b_burst == c_burst) {
            /* Tie-break 1: earlier arrival time */
            if (sjf_ready[i]->arrival_time < sjf_ready[best]->arrival_time)
                best = i;
            /* Tie-break 2: lower PID */
            else if (sjf_ready[i]->arrival_time == sjf_ready[best]->arrival_time &&
                     sjf_ready[i]->pid < sjf_ready[best]->pid)
                best = i;
        }
    }

    Process *chosen = sjf_ready[best];
    /* Remove by swapping with last element */
    sjf_ready[best] = sjf_ready[--sjf_count];
    return chosen;
}

static int sjf_should_preempt(Process *running, Process *arrived) {
    (void)running; (void)arrived;
    return 0;   /* non-preemptive SJF: no preemption on arrival */
}

static int sjf_uses_quantum(void) { return 0; }

static void sjf_reset(void) { sjf_count = 0; }

static Scheduler sjf_sched = {
    .init           = sjf_init,
    .enqueue        = sjf_enqueue,
    .pick_next      = sjf_pick_next,
    .should_preempt = sjf_should_preempt,
    .uses_quantum   = sjf_uses_quantum,
    .reset          = sjf_reset
};

Scheduler *get_sjf_scheduler(void) { return &sjf_sched; }
