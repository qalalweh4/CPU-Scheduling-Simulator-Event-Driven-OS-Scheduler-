/*
 * main.c  —   Abdullah AlQalalweh
 *
 * Entry point. Parses CLI flags, loads workload, selects scheduler,
 * runs simulation, prints results.
 *
 * Usage:
 *   ./scheduler --algo=FCFS --input=workload.txt
 *   ./scheduler --algo=RR   --quantum=4 --input=workload.txt
 */

#include "scheduler.h"

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s --algo=<FCFS|SJF|RR|PRIORITY> --input=<file> [--quantum=<n>]\n",
        prog);
    exit(1);
}

int main(int argc, char *argv[]) {
    char     input_file[256] = "";
    AlgoType algo            = ALGO_FCFS;
    int      quantum         = DEFAULT_QUANTUM;

    /* ── Parse CLI arguments ── */
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--algo=", 7) == 0) {
            const char *a = argv[i] + 7;
            if      (strcmp(a, "FCFS")     == 0) algo = ALGO_FCFS;
            else if (strcmp(a, "SJF")      == 0) algo = ALGO_SJF;
            else if (strcmp(a, "RR")       == 0) algo = ALGO_RR;
            else if (strcmp(a, "PRIORITY") == 0) algo = ALGO_PRIORITY;
            else { fprintf(stderr, "Unknown algo: %s\n", a); usage(argv[0]); }
        }
        else if (strncmp(argv[i], "--input=", 8) == 0) {
            strncpy(input_file, argv[i] + 8, sizeof(input_file) - 1);
        }
        else if (strncmp(argv[i], "--quantum=", 10) == 0) {
            quantum = atoi(argv[i] + 10);
            if (quantum <= 0) { fprintf(stderr, "Quantum must be > 0\n"); exit(1); }
        }
        else {
            fprintf(stderr, "Unknown flag: %s\n", argv[i]);
            usage(argv[0]);
        }
    }

    if (input_file[0] == '\0') {
        fprintf(stderr, "Error: --input is required\n");
        usage(argv[0]);
    }

    /* ── Load workload ── */
    Process procs[MAX_PROCESSES];
    int n = 0;

    if (parse_workload(input_file, procs, &n) != 0) {
        fprintf(stderr, "Failed to parse workload file: %s\n", input_file);
        return 1;
    }
    printf("Loaded %d processes from %s\n\n", n, input_file);

    /* ── Select scheduler ── */
    Scheduler *sched = NULL;
    switch (algo) {
        case ALGO_FCFS:     sched = get_fcfs_scheduler();     break;
        case ALGO_SJF:      sched = get_sjf_scheduler();      break;
        case ALGO_RR:       sched = get_rr_scheduler();       break;
        case ALGO_PRIORITY: sched = get_priority_scheduler(); break;
    }

    /* ── Run simulation ── */
    SimResult result;
    memset(&result, 0, sizeof(result));

    run_simulation(procs, n, sched, quantum, algo, &result);

    /* ── Print results ── */
    print_gantt(&result);
    print_summary(procs, n, &result, algo);
    print_per_process(procs, n);

    return 0;
}
