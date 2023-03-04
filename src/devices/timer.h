#ifndef DEVICES_TIMER_H
#define DEVICES_TIMER_H

#include <round.h>
#include <stdint.h>
#include <list.h>
#include "threads/synch.h"

/* Number of timer interrupts per second. */
#define TIMER_FREQ 100

void timer_init (void);
void timer_calibrate (void);

int64_t timer_ticks (void);
int64_t timer_elapsed (int64_t);

void timer_sleep (int64_t ticks);
void timer_msleep (int64_t milliseconds);
void timer_usleep (int64_t microseconds);
void timer_nsleep (int64_t nanoseconds);

void timer_print_stats (void);

/* ----- Added in lab 2 ----- */
struct sleeping_thread
  {
    struct list_elem elem;
    struct semaphore binary;
    int64_t ticks;
  };

struct list sleep_list;

void add_sorted (struct list_elem *new_elem);
void check_sleeping_threads (void);
void print_sleep_list (void);
void init_sleep_list (void);


#endif /* devices/timer.h */
