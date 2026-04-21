/*
 * output.c  —  HASAN
 *
 * Three print functions:
 *   1. print_gantt()       — ASCII Gantt chart
 *   2. print_summary()     — aggregate metrics table
 *   3. print_per_process() — per-process stats table
 *
 * Required by spec:
 *   - Gantt chart (ASCII)
 *   - Waiting time, turnaround time, response time (per-process + averages)
 *   - CPU utilization, throughput
 *   - Context switch count and overhead
 */

 #include "scheduler.h"

 static const char *algo_name[] = { "FCFS", "SJF", "Round Robin", "Priority" };
 
 /* ─────────────────────────────────────────────
  * INTERNAL HELPER
  * Each Gantt segment is rendered as a fixed-width cell of SEG_W chars.
  * The label is centred inside the cell.  The time number below is
  * right-aligned at the cell boundary.
  *
  *   SEG_W = 6  →  "| P1   |IDLE  | P2   |"
  *                  0     6     12     18
  * ───────────────────────────────────────────── */
 #define SEG_W 7   /* characters per Gantt cell (includes leading '|') */
 
 /* Centre a string of length label_len inside a field of field_w chars,
  * padding with 'pad' on both sides. Writes into buf (must be ≥ field_w+1). */
 static void centre_label(char *buf, int field_w, const char *label, int label_len) {
     int total_pad = field_w - label_len;
     int left      = total_pad / 2;
     int right     = total_pad - left;
     int pos = 0;
     for (int i = 0; i < left;      i++) buf[pos++] = ' ';
     for (int i = 0; i < label_len; i++) buf[pos++] = label[i];
     for (int i = 0; i < right;     i++) buf[pos++] = ' ';
     buf[pos] = '\0';
 }
 
 /* ─────────────────────────────────────────────
  * 1. ASCII Gantt chart
  *
  * Target output (spec example):
  *
  *  | P1    | P2    | P1    | IDLE  | P3    |
  *  0       6       9      13      15      21
  *
  * ───────────────────────────────────────────── */
 void print_gantt(SimResult *result) {
     printf("\n=== Gantt Chart ===\n");
 
     if (result->gantt_len == 0) {
         printf("(no entries)\n\n");
         return;
     }
 
     /* TODO Hasan:
      *
      * Step 1 — Print the top bar of process names:
      *   For each entry in result->gantt[0..gantt_len-1]:
      *     Print "| PX " (or "| IDLE" for pid == -1).
      *   End with "|"
      *
      * Step 2 — Print the timeline below:
      *   Print result->gantt[0].start, then for each entry
      *   print its .end right-aligned at the cell boundary.
      *
      * Implementation note: each cell is SEG_W characters wide.
      * The label (e.g. "P1" or "IDLE") is centred inside SEG_W-1
      * characters (the leading '|' takes 1 char).
      */
 
     /* ── Step 1: top border ── */
     for (int i = 0; i < result->gantt_len; i++) {
         /* top edge of cell */
         printf("+");
         for (int k = 0; k < SEG_W - 1; k++) printf("-");
     }
     printf("+\n");
 
     /* ── Step 1: label row ── */
     for (int i = 0; i < result->gantt_len; i++) {
         GanttEntry *g = &result->gantt[i];
         char label[16];
         int  label_len;
 
         if (g->pid == -1) {
             label_len = snprintf(label, sizeof(label), "IDLE");
         } else {
             label_len = snprintf(label, sizeof(label), "P%d", g->pid);
         }
 
         /* Centre label inside the cell (SEG_W - 1 chars after the '|') */
         char cell[SEG_W + 1];
         centre_label(cell, SEG_W - 1, label, label_len);
         printf("|%s", cell);
     }
     printf("|\n");
 
     /* ── Step 1: bottom border ── */
     for (int i = 0; i < result->gantt_len; i++) {
         printf("+");
         for (int k = 0; k < SEG_W - 1; k++) printf("-");
     }
     printf("+\n");
 
     /* ── Step 2: timeline numbers ──
      * Each boundary i sits at absolute column i * SEG_W.
      * We track `cursor` (chars already printed on this line) and
      * pad with spaces so each number's *last* digit lands exactly
      * on the boundary column.
      */
     {
         int cursor = 0;
 
         /* Collect all boundary times: start of first, then end of each */
         int n_times = result->gantt_len + 1;
         int times[n_times];
         times[0] = result->gantt[0].start;
         for (int i = 0; i < result->gantt_len; i++)
             times[i + 1] = result->gantt[i].end;
 
         for (int i = 0; i < n_times; i++) {
             int boundary_col = i * SEG_W;   /* column where this boundary sits */
             char num[16];
             int  num_len = snprintf(num, sizeof(num), "%d", times[i]);
 
             /* The number must END at boundary_col,
              * so it STARTS at  boundary_col - num_len + 1.
              * But it can never start before cursor. */
             int start_col = boundary_col - num_len + 1;
             if (start_col < cursor) start_col = cursor;
 
             int spaces = start_col - cursor;
             for (int s = 0; s < spaces; s++) putchar(' ');
             printf("%s", num);
             cursor = start_col + num_len;
         }
         printf("\n\n");
     }
 }
 
 
 /* ─────────────────────────────────────────────
  * 2. Summary table
  *
  * Required by spec:
  *   - Waiting time (avg)
  *   - Turnaround time (avg)
  *   - Response time (avg)
  *   - CPU utilization
  *   - Throughput
  *   - Context switch count and overhead (count × CONTEXT_SWITCH units)
  *
  * Example output:
  *
  *  ╔══════════════════════════════════════╗
  *  ║  Summary: Round Robin (quantum=4)   ║
  *  ╠══════════════════════════════════════╣
  *  ║  CPU Utilization   :   87.50 %      ║
  *  ║  Throughput        :   0.3125 p/u   ║
  *  ║  Total Time        :   32           ║
  *  ║  Context Switches  :   12           ║
  *  ║  Context Sw. Cost  :   12 t-units   ║
  *  ╠══════════════════════════════════════╣
  *  ║  Avg Waiting Time  :    4.67        ║
  *  ║  Avg Turnaround    :   10.33        ║
  *  ║  Avg Response Time :    2.00        ║
  *  ╚══════════════════════════════════════╝
  *
  * ───────────────────────────────────────────── */
 void print_summary(Process procs[], int n, SimResult *result, AlgoType algo) {
     /* TODO Hasan:
      *   Print all required metrics:
      *     CPU utilization, throughput, total time,
      *     context switch count, context switch overhead,
      *     avg waiting, avg turnaround, avg response.
      */
 
     /* ── Build header string ── */
     char header[64];
     if (algo == ALGO_RR)
         snprintf(header, sizeof(header), "Summary: %s (quantum=%d)",
                  algo_name[algo], result->quantum);
     else
         snprintf(header, sizeof(header), "Summary: %s", algo_name[algo]);
 
     /* ── Compute per-process averages ── */
     double avg_wait = 0.0, avg_turn = 0.0, avg_resp = 0.0;
     int    done = 0;
     for (int i = 0; i < n; i++) {
         if (procs[i].state == STATE_DONE) {
             avg_wait += procs[i].waiting_time;
             avg_turn += procs[i].turnaround_time;
             avg_resp += procs[i].response_time;
             done++;
         }
     }
     if (done > 0) {
         avg_wait /= done;
         avg_turn /= done;
         avg_resp /= done;
     }
 
     /* Context switch overhead = count × CONTEXT_SWITCH time units */
     int cs_overhead = result->context_switches * CONTEXT_SWITCH;
 
     /* ── Print box ── */
     printf("\n+----------------------------------------------+\n");
     printf("|  %-44s|\n", header);
     printf("+----------------------------------------------+\n");
     printf("|  CPU Utilization   : %6.2f %%                |\n",
            result->cpu_utilization * 100.0);
     printf("|  Throughput        : %8.4f proc/time-unit |\n",
            result->throughput);
     printf("|  Total Time        : %4d time units         |\n",
            result->total_time);
     printf("|  Context Switches  : %4d                    |\n",
            result->context_switches);
     printf("|  Context Sw. Cost  : %4d time units         |\n",
            cs_overhead);
     printf("+----------------------------------------------+\n");
     printf("|  Avg Waiting Time  : %7.2f time units      |\n", avg_wait);
     printf("|  Avg Turnaround    : %7.2f time units      |\n", avg_turn);
     printf("|  Avg Response Time : %7.2f time units      |\n", avg_resp);
     printf("+----------------------------------------------+\n\n");
 }
 
 
 /* ─────────────────────────────────────────────
  * 3. Per-process stats table
  *
  * Required by spec: waiting time, turnaround time, response time per process.
  *
  * Example output:
  *
  *  === Per-Process Stats ===
  *  PID  Arrival  Finish  Waiting  Turnaround  Response
  *  ---  -------  ------  -------  ----------  --------
  *    1        0       8        2           8         0
  *    2        2      14        5          12         1
  *
  * ───────────────────────────────────────────── */
 void print_per_process(Process procs[], int n) {
     printf("=== Per-Process Stats ===\n");
     printf("%-5s %-8s %-8s %-9s %-12s %-9s\n",
            "PID", "Arrival", "Finish", "Waiting", "Turnaround", "Response");
     printf("%-5s %-8s %-8s %-9s %-12s %-9s\n",
            "-----", "--------", "--------", "---------", "------------", "---------");
 
     /* TODO Hasan:
      *   For each procs[i] where state == STATE_DONE:
      *     printf the six fields below.
      *   For unfinished processes print N/A in numeric columns.
      */
     for (int i = 0; i < n; i++) {
         Process *p = &procs[i];
         if (p->state == STATE_DONE) {
             printf("%-5d %-8d %-8d %-9d %-12d %-9d\n",
                    p->pid,
                    p->arrival_time,
                    p->finish_time,
                    p->waiting_time,
                    p->turnaround_time,
                    p->response_time);
         } else {
             /* Process did not finish — indicate incomplete */
             printf("%-5d %-8d %-8s %-9s %-12s %-9s\n",
                    p->pid, p->arrival_time,
                    "N/A", "N/A", "N/A", "N/A");
         }
     }
     printf("\n");
 }