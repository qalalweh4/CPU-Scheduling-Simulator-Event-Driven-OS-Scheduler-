CC      = gcc
CFLAGS  = -Wall -Wextra -g -std=c11

SRCS    = main.c engine.c parser.c event_queue.c \
          algo_fcfs_sjf.c algo_rr_priority.c output.c

OBJS    = $(SRCS:.c=.o)
TARGET  = scheduler

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c scheduler.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

run_fcfs:
	./$(TARGET) --algo=FCFS --input=workload1.txt

run_sjf:
	./$(TARGET) --algo=SJF --input=workload1.txt

run_rr:
	./$(TARGET) --algo=RR --quantum=4 --input=workload2.txt

run_priority:
	./$(TARGET) --algo=PRIORITY --input=workload3.txt

.PHONY: all clean run_fcfs run_sjf run_rr run_priority
