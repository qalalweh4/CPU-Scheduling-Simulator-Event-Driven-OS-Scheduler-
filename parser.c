/*
 * parser.c  —   Abdullah AlQalalweh
 *
 * Reads workload files in this format (one process per line):
 *
 *   PID ARRIVAL PRIORITY CPU1 IO1 CPU2 IO2 ... CPUn
 *
 * Lines starting with '#' are comments.
 *
 * Example:
 *   # PID  ARR  PRI  CPU IO  CPU IO  CPU
 *     1    0    2    6   2   4   1   3
 *     2    2    1    3   1   2
 *     3    4    3    8
 */

#include "scheduler.h"

int parse_workload(const char *filename, Process procs[], int *n) {
    FILE *f = fopen(filename, "r");
    if (!f) { perror(filename); return -1; }

    *n = 0;
    char line[512];

    while (fgets(line, sizeof(line), f)) {
        /* Skip comments and blank lines */
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        if (*n >= MAX_PROCESSES) {
            fprintf(stderr, "Warning: max process limit (%d) reached\n", MAX_PROCESSES);
            break;
        }

        Process *p = &procs[*n];
        memset(p, 0, sizeof(*p));

        /* Parse PID, arrival, priority */
        char *tok = strtok(line, " \t\r\n");
        if (!tok) continue;
        p->pid = atoi(tok);

        tok = strtok(NULL, " \t\r\n");
        if (!tok) continue;
        p->arrival_time = atoi(tok);

        tok = strtok(NULL, " \t\r\n");
        if (!tok) continue;
        p->priority = atoi(tok);

        /* Parse alternating CPU and I/O bursts:
         *   token 0 → cpu_bursts[0]
         *   token 1 → io_bursts[0]
         *   token 2 → cpu_bursts[1]
         *   token 3 → io_bursts[1]  ... etc.
         */
        int burst_count = 0;

        while ((tok = strtok(NULL, " \t\r\n")) != NULL) {
            if (burst_count >= MAX_BURSTS * 2) break;
            int val = atoi(tok);
            if (burst_count % 2 == 0) {
                /* Even index → CPU burst */
                p->cpu_bursts[p->num_cpu_bursts++] = val;
            } else {
                /* Odd index → I/O burst (associated with the preceding CPU burst) */
                p->io_bursts[(burst_count - 1) / 2] = val;
            }
            burst_count++;
        }

        if (p->num_cpu_bursts == 0) {
            fprintf(stderr, "Warning: PID %d has no CPU bursts, skipping\n", p->pid);
            continue;
        }

        /* Initialise runtime fields */
        p->state           = STATE_NEW;
        p->remaining_burst = p->cpu_bursts[0];
        p->burst_index     = 0;
        p->start_time      = -1;

        (*n)++;
    }

    fclose(f);
    return 0;
}
