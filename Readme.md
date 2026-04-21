# CPU Scheduling Simulator

An event-driven CPU scheduling simulator written in C.
Supports four algorithms, I/O bursts, preemption, and produces
Gantt charts + per-process statistics.

---

## Team

| Member | Role | Files |
|--------|------|-------|
| You (leader) | Core engine, CLI, parser, event queue, integration | `main.c`, `engine.c`, `parser.c`, `event_queue.c`, `scheduler.h` |
| Ahmed | FCFS + SJF schedulers | `algo_fcfs_sjf.c` |
| Turki | Round Robin + Priority schedulers | `algo_rr_priority.c` |
| Hasan | Output: Gantt chart, summary table, per-process stats | `output.c` |

---

## Build

```bash
make          # builds ./scheduler
make clean    # removes objects and binary
```

Requires: `gcc`, `make`, C11 standard (`-std=c11`).

---

## Usage

```bash
./scheduler --algo=<FCFS|SJF|RR|PRIORITY> --input=./workloads/<workload_file> [--quantum=<n>]
```

### Examples

```bash
./scheduler --algo=FCFS     --input=./workloads/workload1.txt
./scheduler --algo=SJF      --input=./workloads/workload1.txt
./scheduler --algo=RR       --quantum=4 --input=./workloads/workload2.txt
./scheduler --algo=PRIORITY --input=./workloads/workload3.txt
```

Or use the Makefile shortcuts:

```bash
make run_fcfs
make run_sjf
make run_rr
make run_priority
```

---

## Workload File Format

One process per line. Lines starting with `#` are comments.

```
PID  ARRIVAL  PRIORITY  CPU1 [IO1 CPU2 IO2 ... CPUn]
```

- **PID** — process identifier (integer)
- **ARRIVAL** — arrival time in time units
- **PRIORITY** — scheduling priority (lower number = higher urgency)
- **CPU/IO bursts** — alternating: CPU burst, I/O burst, CPU burst, ...
  The last value is always a CPU burst (no trailing I/O).

### Example

```
# PID  ARR  PRI  CPU  IO  CPU  IO  CPU
  1    0    2    6    2   4    1   3
  2    2    1    3    1   2
  3    4    3    8
```

Process 1 has three CPU bursts (6, 4, 3) with two I/O bursts (2, 1) between them.
Process 3 has a single CPU burst of 8 with no I/O.

---

## Sample Workloads

| File | Description | Best used with |
|------|-------------|----------------|
| `workload1.txt` | 5 processes, CPU-only, no I/O | FCFS, SJF |
| `workload2.txt` | 5 processes with I/O bursts | RR, all algorithms |
| `workload3.txt` | Priority starvation scenario | PRIORITY vs RR comparison |

---

## Output

### Gantt Chart
```
=== Gantt Chart ===
| P1--| P2-| P1--| P3------|
0    6    9   13          21
```

### Summary Table
```
=== Summary: Round Robin (quantum=4) ===
CPU Utilization : 87.50%
Throughput      : 0.1563 proc/unit
Context Switches: 12
Total Time      : 32
Avg Waiting     : 4.67
Avg Turnaround  : 10.33
Avg Response    : 2.00
```

### Per-Process Stats
```
=== Per-Process Stats ===
PID  Arrival  Finish  Waiting  Turnaround  Response
---  -------  ------  -------  ----------  --------
1    0        8       2        8           0
2    2        14      5        12          1
```

---

## Architecture

```
main.c
  └── parse_workload()       [parser.c]
  └── get_xxx_scheduler()    [algo_fcfs_sjf.c / algo_rr_priority.c]
  └── run_simulation()       [engine.c]
        └── eq_insert/pop()  [event_queue.c]
        └── sched->enqueue() / pick_next() / should_preempt() / uses_quantum()
  └── print_gantt()          [output.c]
  └── print_summary()        [output.c]
  └── print_per_process()    [output.c]
```

### Scheduler vtable (in `scheduler.h`)

Each algorithm implements five function pointers:

| Function | Called by engine | Purpose |
|----------|-----------------|---------|
| `init(quantum)` | Once at start | Reset internal queues, store quantum |
| `enqueue(p)` | On arrival / I/O done / preemption | Add process to ready queue |
| `pick_next()` | When CPU is free | Choose next process to run |
| `should_preempt(running, arrived)` | On every arrival & I/O done | Return 1 to preempt (Priority only) |
| `uses_quantum()` | When dispatching | Return 1 to enable RR time-slice |
| `reset()` | At end of simulation | Clear state for re-use |

### Event types

| Event | Meaning |
|-------|---------|
| `EVT_ARRIVE` | Process enters the system |
| `EVT_CPU_DONE` | CPU burst completed |
| `EVT_IO_DONE` | I/O burst completed, process re-enters ready queue |
| `EVT_PREEMPT` | RR quantum expired — process is re-enqueued |

---

## Algorithms

### FCFS (Ahmed)
Pure FIFO. Processes run to completion in arrival order. No preemption.

### SJF (Ahmed)
Non-preemptive. On each dispatch, picks the ready process with the shortest
remaining CPU burst. Ties broken by arrival time, then PID.

### Round Robin (Turki)
Time-sliced. Each process gets at most `quantum` time units before being
re-enqueued at the tail. Preemption triggered by `EVT_PREEMPT` in the engine.

### Priority (Turki)
Preemptive. Lower priority number = higher urgency. A newly arriving or
I/O-returning process immediately preempts the running process if it has
a smaller priority number. Ties broken by arrival time, then PID.

---

## Metrics Computed

| Metric | Formula |
|--------|---------|
| Turnaround time | `finish_time − arrival_time` |
| Waiting time | `turnaround_time − Σ(cpu_bursts)` |
| Response time | `first_cpu_time − arrival_time` |
| CPU utilization | `(total_time − idle_time) / total_time` |
| Throughput | `n / total_time` |
