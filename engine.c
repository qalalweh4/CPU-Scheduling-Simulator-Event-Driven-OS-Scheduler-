/*
 * engine.c  —  Abdullah AlQalalweh
 *
 * Event-driven simulation loop.
 * Drives all schedulers through the same timeline.
 */

#include "scheduler.h"

/* ── helper: add a Gantt entry, merging adjacent identical slots ── */
static void gantt_push(SimResult *r, int pid, int start, int end) {
    if (start == end) return;   /* zero-length slice — skip */
    if (r->gantt_len >= MAX_GANTT) return;

    /* Merge with previous entry if same PID and contiguous */
    if (r->gantt_len > 0) {
        GanttEntry *prev = &r->gantt[r->gantt_len - 1];
        if (prev->pid == pid && prev->end == start) {
            prev->end = end;
            return;
        }
    }
    r->gantt[r->gantt_len++] = (GanttEntry){ pid, start, end };
}

/* ── helper: find a process by PID ── */
static Process *find_proc(Process procs[], int n, int pid) {
    for (int i = 0; i < n; i++)
        if (procs[i].pid == pid) return &procs[i];
    return NULL;
}

/* ── helper: sum all cpu bursts for a process (for waiting time calc) ── */
static int total_cpu_time(Process *p) {
    int total = 0;
    for (int i = 0; i < p->num_cpu_bursts; i++)
        total += p->cpu_bursts[i];
    return total;
}

void run_simulation(Process procs[], int n, Scheduler *sched,
                    int quantum, AlgoType algo, SimResult *result) {

    sched->init(quantum);
    result->algo    = algo;
    result->quantum = (algo == ALGO_RR) ? quantum : 0;

    Event    *eq         = NULL;   /* event queue (sorted by time) */
    int       clock      = 0;
    int       idle_time  = 0;
    int       done_count = 0;
    Process  *running    = NULL;
    int       run_start  = 0;
    int       preempt_pid = -1;    /* PID of pending PREEMPT event, -1 if none */

    /* ── Reset all process state before simulation ── */
    for (int i = 0; i < n; i++) {
        procs[i].state           = STATE_NEW;
        procs[i].remaining_burst = procs[i].cpu_bursts[0];
        procs[i].burst_index     = 0;
        procs[i].start_time      = -1;
        procs[i].finish_time     = 0;
        procs[i].waiting_time    = 0;
        procs[i].turnaround_time = 0;
        procs[i].response_time   = 0;
    }

    /* ── Seed arrival events for all processes ── */
    for (int i = 0; i < n; i++) {
        Event *e = eq_create(procs[i].arrival_time, EVT_ARRIVE, procs[i].pid);
        eq_insert(&eq, e);
    }

    /* ── Main event loop ── */
    while (done_count < n) {
        Event *e = eq_pop(&eq);

        /* If no event but processes remain, we have an idle gap —
           advance clock to next arrival that might be pending.
           This handles the edge case where all events are exhausted
           but some processes have not finished (shouldn't happen with
           correct workload, but guards infinite loop). */
        if (!e) break;

        clock = e->time;
        Process *p = find_proc(procs, n, e->pid);

        switch (e->type) {

            /* ── A new process enters the ready queue ── */
            case EVT_ARRIVE:
                p->state = STATE_READY;
                sched->enqueue(p);

                /* Preempt running process if needed (Priority scheduling) */
                if (running && sched->should_preempt(running, p)) {
                    gantt_push(result, running->pid, run_start, clock);
                    result->context_switches++;

                    /* Cancel any pending PREEMPT event for this process */
                    preempt_pid = -1;

                    running->state = STATE_READY;
                    sched->enqueue(running);
                    running = NULL;
                }
                break;

            /* ── A CPU burst has finished ── */
            case EVT_CPU_DONE:
                /* Guard: stale event after a preemption */
                if (!running || running->pid != e->pid) break;

                gantt_push(result, running->pid, run_start, clock);
                preempt_pid = -1;
                running->burst_index++;

                if (running->burst_index >= running->num_cpu_bursts) {
                    /* Process is completely finished */
                    running->finish_time     = clock;
                    running->turnaround_time = clock - running->arrival_time;
                    running->state           = STATE_DONE;
                    done_count++;
                } else {
                    /* More bursts remain — go do I/O next */
                    int io_idx = running->burst_index - 1;
                    int io_len = running->io_bursts[io_idx];
                    running->state = STATE_WAITING;
                    Event *io_done = eq_create(clock + io_len, EVT_IO_DONE, running->pid);
                    eq_insert(&eq, io_done);
                }
                result->context_switches++;
                running = NULL;
                break;

            /* ── I/O burst finished; process re-enters ready queue ── */
            case EVT_IO_DONE:
                p->remaining_burst = p->cpu_bursts[p->burst_index];
                p->state = STATE_READY;
                sched->enqueue(p);

                /* Preempt running if this returning process has higher priority */
                if (running && sched->should_preempt(running, p)) {
                    gantt_push(result, running->pid, run_start, clock);
                    result->context_switches++;
                    preempt_pid = -1;
                    running->state = STATE_READY;
                    sched->enqueue(running);
                    running = NULL;
                }
                break;

            /* ── Round Robin quantum expiry — Turki hooks this via uses_quantum() ── */
            case EVT_PREEMPT:
                /* Ignore stale preempt events (process already left CPU) */
                if (!running || running->pid != e->pid) break;

                gantt_push(result, running->pid, run_start, clock);
                result->context_switches++;
                preempt_pid = -1;

                running->state = STATE_READY;
                sched->enqueue(running);
                running = NULL;
                break;

            default:
                break;
        }
        free(e);

        /* ── Dispatch next process if CPU is free ── */
        if (!running) {
            running = sched->pick_next();
            if (running) {
                run_start = clock;

                /* Record first-time response */
                if (running->start_time == -1) {
                    running->start_time    = clock;
                    running->response_time = clock - running->arrival_time;
                }
                running->state = STATE_RUNNING;

                int burst_len = running->remaining_burst;

                /* Schedule CPU_DONE or RR PREEMPT event */
                if (sched->uses_quantum() && burst_len > quantum) {
                    /* TODO: Turki — hook quantum preemption here.
                     * When the quantum expires we fire EVT_PREEMPT.
                     * remaining_burst is reduced by quantum so the next
                     * slice knows how much is left. */
                    running->remaining_burst -= quantum;
                    Event *preempt = eq_create(clock + quantum, EVT_PREEMPT, running->pid);
                    eq_insert(&eq, preempt);
                    preempt_pid = running->pid;
                } else {
                    /* Burst fits entirely in one slice (or non-RR algo) */
                    running->remaining_burst = 0;
                    Event *done = eq_create(clock + burst_len, EVT_CPU_DONE, running->pid);
                    eq_insert(&eq, done);
                    preempt_pid = -1;
                }
            } else {
                /* CPU idle — find the next event time to measure idle gap */
                if (eq) {
                    /* Peek at head without consuming */
                    int next_time = eq->time;
                    if (next_time > clock) {
                        gantt_push(result, -1, clock, next_time);
                        idle_time += next_time - clock;
                    }
                }
            }
        }
    }

    /* ── Compute summary metrics ── */
    result->total_time      = clock;
    result->cpu_utilization = (clock > 0)
        ? (double)(clock - idle_time) / clock
        : 0.0;
    result->throughput      = (clock > 0)
        ? (double)n / clock
        : 0.0;

    /* Compute per-process waiting time:
     *   waiting_time = turnaround_time - (sum of all CPU bursts) - (sum of all I/O bursts)
     * Simpler: waiting_time = turnaround_time - total_cpu_time
     * (I/O time does not count as waiting for the CPU)
     */
    for (int i = 0; i < n; i++) {
        if (procs[i].state == STATE_DONE) {
            /* TODO: sum all bursts — this now correctly sums every cpu burst,
             * not just the first one (the original skeleton only subtracted cpu_bursts[0]) */
            procs[i].waiting_time =
                procs[i].turnaround_time - total_cpu_time(&procs[i]);
            if (procs[i].waiting_time < 0)
                procs[i].waiting_time = 0;
        }
    }

    eq_free(&eq);
    sched->reset();
    (void)preempt_pid;  /* suppress unused-variable warning */
}
