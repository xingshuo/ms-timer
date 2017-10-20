#ifndef _TIMER_H
#define _TIMER_H
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <sys/time.h>
#include <string.h>

#define HEAP0 1

#define ADD_TIME(tvp, uvp, vvp)          \
  do {                \
    (vvp)->tv_sec = (tvp)->tv_sec + (uvp)->tv_sec;    \
    (vvp)->tv_usec = (tvp)->tv_usec + (uvp)->tv_usec;       \
    if ((vvp)->tv_usec >= 1000000) {      \
      (vvp)->tv_sec++;        \
      (vvp)->tv_usec -= 1000000;      \
    }             \
  } while (0)

#define SUB_TIME(tvp, uvp, vvp)                  \
    do {                                \
        (vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;      \
        (vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;   \
        if ((vvp)->tv_usec < 0) {               \
            (vvp)->tv_sec--;                \
            (vvp)->tv_usec += 1000000;          \
        }                           \
    } while (0)

#define CMP_TIME(tvp, uvp, cmp)                  \
    (((tvp)->tv_sec == (uvp)->tv_sec) ?             \
     ((tvp)->tv_usec cmp (uvp)->tv_usec) :              \
     ((tvp)->tv_sec cmp (uvp)->tv_sec))

#define GET_TIME(tp) \
    do {    \
        gettimeofday(tp, (struct timezone *) NULL); \
    } while (0)

#define TOP_TIME(tb)  ((tb)->data[HEAP0].deadline)

typedef struct {
  int session;
  int count;
  struct timeval interval;
  struct timeval deadline;
} Timer;

typedef struct {
    Timer* data;
    size_t size;
    size_t capacity;
    void(*push)(void*, int, int, int);
    void(*adjust)(void*, int, int, int);
    void(*erase)(void*, int);
} TimerBase;

TimerBase* create_timer(size_t);
void release_timer(TimerBase*);
int process_timer(TimerBase*, int*, int);

#endif