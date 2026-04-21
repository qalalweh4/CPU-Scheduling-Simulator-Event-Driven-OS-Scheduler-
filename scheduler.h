#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* ─────────────────────────────────────────────
   CONSTANTS
───────────────────────────────────────────── */
#define MAX_PROCESSES     64
#define MAX_BURSTS        32
#define MAX_GANTT         4096
#define DEFAULT_QUANTUM   4
#define CONTEXT_SWITCH    1   /* time units overhead per context switch */

/* ─────────────────────────────────────────────
   ENUMS
───────────────────────────────────────────── */
typedef enum {
    ALGO_FCFS     = 0,
    ALGO_SJF      = 1,
    ALGO_RR       = 2,
    ALGO_PRIORITY = 3
} AlgoType;

typedef enum {
    STATE_NEW,
    STATE_READY,
    STATE_RUNNING,
    STATE_WAITING,   /* in I/O */
    STATE_DONE
} ProcessState;

/* ─────────────────────────────────────────────
   PROCESS / PCB
───────────────────────────────────────────── */
typedef struct {
    int pid;
    int arrival_time;
    int priority;           /* lower number = higher priority */
    int num_cpu_bursts;
    int cpu_bursts[MAX_BURSTS];
    int io_bursts[MAX_BURSTS];

    /* --- filled in by scheduler --- */
    ProcessState state;
    int remaining_burst;    /* remaining time in current CPU burst */
    int burst_index;        /* which burst we are on */
    int finish_time;
    int start_time;         /* first time it got the CPU */
    int waiting_time;
    int turnaround_time;
    int response_time;
} Process;

/* ─────────────────────────────────────────────
   EVENT (for event-driven engine)
───────────────────────────────────────────── */
typedef enum {
    EVT_ARRIVE,
    EVT_DISPATCH,
    EVT_CPU_DONE,
    EVT_IO_DONE,
    EVT_PREEMPT
} EventType;

typedef struct Event {
    int         time;
    EventType   type;
    int         pid;
    struct Event *next;     /* sorted linked list */
} Event;

/* ─────────────────────────────────────────────
   GANTT CHART ENTRY
───────────────────────────────────────────── */
typedef struct {
    int pid;        /* -1 = idle */
    int start;
    int end;
} GanttEntry;

/* ─────────────────────────────────────────────
   SIMULATION RESULT (collected by engine, printed by Hasan)
───────────────────────────────────────────── */
typedef struct {
    GanttEntry gantt[MAX_GANTT];
    int        gantt_len;
    int        context_switches;
    int        total_time;
    double     cpu_utilization;   /* 0.0 – 1.0 */
    double     throughput;        /* processes / time unit */
    AlgoType   algo;              /* stored so output.c can label correctly */
    int        quantum;           /* quantum used (RR only, else 0) */
} SimResult;

/* ─────────────────────────────────────────────
   SCHEDULER INTERFACE  (implemented per algorithm)
   Each algorithm fills in this vtable.
───────────────────────────────────────────── */
typedef struct {
    /* Called once before simulation starts */
    void (*init)(int quantum);

    /* Called when a process enters the ready queue */
    void (*enqueue)(Process *p);

    /* Called to pick the next process to run; returns NULL if idle */
    Process *(*pick_next)(void);

    /* Called on preemption check: should running be preempted by arrived? */
    int (*should_preempt)(Process *running, Process *arrived);

    /* Returns 1 if this scheduler uses time-quantum preemption (RR) */
    int (*uses_quantum)(void);

    /* Clean up internal state between runs */
    void (*reset)(void);
} Scheduler;

/* ─────────────────────────────────────────────
   FUNCTION PROTOTYPES
───────────────────────────────────────────── */

/* --- parser.c (YOU) --- */
int  parse_workload(const char *filename, Process procs[], int *n);

/* --- engine.c (YOU) --- */
void run_simulation(Process procs[], int n, Scheduler *sched,
                    int quantum, AlgoType algo, SimResult *result);

/* --- algo_fcfs_sjf.c (Ahmed) --- */
Scheduler *get_fcfs_scheduler(void);
Scheduler *get_sjf_scheduler(void);

/* --- algo_rr_priority.c (Turki) --- */
Scheduler *get_rr_scheduler(void);
Scheduler *get_priority_scheduler(void);

/* --- output.c (Hasan) --- */
void print_gantt(SimResult *result);
void print_summary(Process procs[], int n, SimResult *result, AlgoType algo);
void print_per_process(Process procs[], int n);

/* --- event_queue.c (YOU) --- */
Event *eq_create(int time, EventType type, int pid);
void   eq_insert(Event **head, Event *e);
Event *eq_pop(Event **head);
void   eq_free(Event **head);

#endif /* SCHEDULER_H */
