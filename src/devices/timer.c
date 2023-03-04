#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "threads/interrupt.h"
#include "threads/io.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include <list.h>
  
/* See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* Keeps track if the sleep_list has been initialized. */
static bool init = false;

/* Number of timer ticks since OS booted. */
static int64_t ticks;

/* Number of loops per timer tick.
   Initialized by timer_calibrate(). */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);

/* Sets up the 8254 Programmable Interval Timer (PIT) to
   interrupt PIT_FREQ times per second, and registers the
   corresponding interrupt. */
void
timer_init (void) 
{
  /* 8254 input frequency divided by TIMER_FREQ, rounded to
     nearest. */
  uint16_t count = (1193180 + TIMER_FREQ / 2) / TIMER_FREQ;

  outb (0x43, 0x34);    /* CW: counter 0, LSB then MSB, mode 2, binary. */
  outb (0x40, count & 0xff);
  outb (0x40, count >> 8);

  intr_register_ext (0x20, timer_interrupt, "8254 Timer");
}

/* Calibrates loops_per_tick, used to implement brief delays. */
void
timer_calibrate (void) 
{
  unsigned high_bit, test_bit;

  ASSERT (intr_get_level () == INTR_ON);
  printf ("Calibrating timer...  ");

  /* Approximate loops_per_tick as the largest power-of-two
     still less than one timer tick. */
  loops_per_tick = 1u << 10;
  while (!too_many_loops (loops_per_tick << 1)) 
    {
      loops_per_tick <<= 1;
      ASSERT (loops_per_tick != 0);
    }

  /* Refine the next 8 bits of loops_per_tick. */
  high_bit = loops_per_tick;
  for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
    if (!too_many_loops (high_bit | test_bit))
      loops_per_tick |= test_bit;

  printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/* Returns the number of timer ticks since the OS booted. */
int64_t
timer_ticks (void) 
{
  enum intr_level old_level = intr_disable ();
  int64_t t = ticks;
  intr_set_level (old_level);
  barrier ();
  return t;
}

/* Returns the number of timer ticks elapsed since THEN, which
   should be a value once returned by timer_ticks(). */
int64_t
timer_elapsed (int64_t then) 
{
  return timer_ticks () - then;
}

/* Suspends execution for approximately TICKS timer ticks. */
void
timer_sleep (int64_t ticks) 
{
  /* Create a sleeping_thread for every thread that is put to
   sleep. The current thread is saved in the sleeping_threads
   semaphore. */
  struct sleeping_thread new;
  new.ticks = timer_ticks () + ticks;
  add_sorted (&new.elem);
  sema_init (&new.binary, 0);
  sema_down (&new.binary);
}

/* Suspends execution for approximately MS milliseconds. */
void
timer_msleep (int64_t ms) 
{
  real_time_sleep (ms, 1000);
}

/* Suspends execution for approximately US microseconds. */
void
timer_usleep (int64_t us) 
{
  real_time_sleep (us, 1000 * 1000);
}

/* Suspends execution for approximately NS nanoseconds. */
void
timer_nsleep (int64_t ns) 
{
  real_time_sleep (ns, 1000 * 1000 * 1000);
}

/* Prints timer statistics. */
void
timer_print_stats (void) 
{
  printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}

/* Timer interrupt handler. */
static void
timer_interrupt (struct intr_frame *args UNUSED)
{
  ticks++;
  thread_tick ();
  check_sleeping_threads ();
}

/* Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false. */
static bool
too_many_loops (unsigned loops) 
{
  /* Wait for a timer tick. */
  int64_t start = ticks;
  while (ticks == start)
    barrier ();

  /* Run LOOPS loops. */
  start = ticks;
  busy_wait (loops);

  /* If the tick count changed, we iterated too long. */
  barrier ();
  return start != ticks;
}

/* Iterates through a simple loop LOOPS times, for implementing
   brief delays.

   Marked NO_INLINE because code alignment can significantly
   affect timings, so that if this function was inlined
   differently in different places the results would be difficult
   to predict. */
static void NO_INLINE
busy_wait (int64_t loops) 
{
  while (loops-- > 0)
    barrier ();
}

/* Sleep for approximately NUM/DENOM seconds. */
static void
real_time_sleep (int64_t num, int32_t denom) 
{
  /* Convert NUM/DENOM seconds into timer ticks, rounding down.
          
        (NUM / DENOM) s          
     ---------------------- = NUM * TIMER_FREQ / DENOM ticks. 
     1 s / TIMER_FREQ ticks
  */
  int64_t ticks = num * TIMER_FREQ / denom;

  ASSERT (intr_get_level () == INTR_ON);
  if (ticks > 0)
    {
      /* We're waiting for at least one full timer tick.  Use
         timer_sleep() because it will yield the CPU to other
         processes. */                
      timer_sleep (ticks); 
    }
  else 
    {
      /* Otherwise, use a busy-wait loop for more accurate
         sub-tick timing.  We scale the numerator and denominator
         down by 1000 to avoid the possibility of overflow. */
      ASSERT (denom % 1000 == 0);
      busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000)); 
    }
}

/* Checks all the sleeping threads in sleep_list and checks
   if any of them should be awakened. If there are any, awake the
   threads with sema_up() from their related semaphore. */
void
check_sleeping_threads ()
{
  /* Try to initialize sleep_list. If already done, nothing happens. */
  init_sleep_list ();
  while (!list_empty (&sleep_list))
  {
    struct list_elem *e = list_front (&sleep_list);
    struct sleeping_thread *s = list_entry
    (e, struct sleeping_thread, elem);
    if (s->ticks > timer_ticks ())
    {
      /* Stop looping if ticks are bigger than timer_ticks.
         Works because the sleep_list is sorted. */
      break;
    }
    else
    {
      /* Remove all elements from sleep_list whose ticks are
         bigger than timer_ticks (which means the thread is
         done sleeping). */
      e = list_pop_front (&sleep_list);
      /* Release the sleeping thread. */
      sema_up (&s->binary);
    }
  }
}

/* This function is called everytime timer_sleep() is used on a thread.
   The function puts the element to the new struct sleeping_thread
   (that's related to the current thread that is put to sleep)
   in the sleep_list sorted after the amount of ticks it should sleep. */
void
add_sorted (struct list_elem *new_elem)
{
  /* Try to initialize sleep_list. If already done, nothing happens. */
  init_sleep_list ();
  /* If sleep_list is empty, just add the element. */
  if (list_empty (&sleep_list))
  {
    list_push_back (&sleep_list, new_elem);
  }
  else
  {
    struct sleeping_thread *new_struct = list_entry
        (new_elem, struct sleeping_thread, elem);
    struct list_elem *e = list_front (&sleep_list);
    struct sleeping_thread *s = list_entry
    (e, struct sleeping_thread, elem);
    /* Loop until the last element is reached or
       new_struct's ticks is smaller than an elements ticks. */
    while (e->next != NULL && new_struct->ticks > s->ticks)
    {
      e = e->next;
      s = list_entry (e, struct sleeping_thread, elem);
    }
    /* Insert the new element in sleep_list. */
    list_insert (e, new_elem);
  }
}

/* Prints the ticks related to the elements in the sleep_list.
   Prints nothing if no elements are in sleep_list. */
void
print_sleep_list () {
  printf("Printing sleep_list\n");
  struct list_elem *e;
  e = list_front (&sleep_list);
  while (e->next != NULL)
  {
    /* Loop until last element. */
    struct sleeping_thread *s = list_entry
    (e, struct sleeping_thread, elem);
    printf("%" PRId64 "\n", s->ticks);
    e = e->next;
  }
}

/* Initializes the sleep_list. Only happens once when init is false. */
void
init_sleep_list () {
  if (!init) {
    list_init (&sleep_list);
    /* Put init to false so sleep_list isn't initialized again. */
    init = true;
  }
}