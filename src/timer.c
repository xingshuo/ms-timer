#include "timer.h"

#define HPARENT(k) ((k) >> 1)

#define EARLY_THAN(x, y) CMP_TIME(&(x.deadline), &(y.deadline), <=)

void
downheap (Timer *heap, int N, int k)
{
  Timer he = heap [k];

  for (;;)
    {
      int c = k << 1;

      if (c >= N + HEAP0)
        break;

      c += c + 1 < N + HEAP0 && !EARLY_THAN(heap [c], heap [c + 1])
           ? 1 : 0;

      if (EARLY_THAN(he, heap[c]))
        break;

      heap [k] = heap [c];
      
      k = c;
    }

  heap [k] = he;
}

void
upheap (Timer *heap, int k)
{
  Timer he = heap [k];

  for (;;)
    {
      int p = HPARENT (k);

      if ((!p) || EARLY_THAN(heap[p], he))
        break;

      heap [k] = heap [p];
      k = p;
    }

  heap [k] = he;
}

void
adjustheap (Timer *heap, int N, int k)
{
  if (k > HEAP0 && EARLY_THAN(heap[k], heap[ HPARENT(k) ]))
    upheap (heap, k);
  else
    downheap (heap, N, k);
}

void
reheap (Timer *heap, int N)
{
  int i;
  for (i = 0; i < N; ++i)
    upheap (heap, i + HEAP0);
}


static void
push_timer(void *arg, int session, int msec, int count) {
  TimerBase *TI = (TimerBase*)arg;
  if (TI->size >= TI->capacity) {
    int old_cap = TI->capacity;
    TI->capacity = 2*old_cap;
    TI->data = realloc(TI->data, (TI->capacity+1)*sizeof(Timer));
    memset(TI->data+old_cap+1, 0, old_cap*sizeof(Timer));
  }
  Timer *p = &TI->data[++TI->size];
  p->session = session;
  GET_TIME(&p->deadline);
  int tv_sec = msec/1000;
  int tv_usec = (msec%1000)*1000;
  struct timeval interval = {tv_sec, tv_usec};
  ADD_TIME(&p->deadline, &interval, &p->deadline);
  p->interval = interval;
  p->count = (count > 0) ? count : -1;
  upheap(TI->data, TI->size);
}

TimerBase*
create_timer(size_t capacity) {
  assert(capacity > 0);
  TimerBase *TI = malloc(sizeof(*TI));
  TI->capacity = capacity;
  TI->size = 0;
  TI->data = calloc(TI->capacity+1, sizeof(Timer));
  TI->push = push_timer;
  return TI;
}

void
release_timer(TimerBase* m) {
  free(m->data);
  free(m);
}

int
process_timer(TimerBase *TI, int *outbuffer, int maxsize) {
  int n = 0;
  struct timeval cur_tv;
  GET_TIME(&cur_tv);
  Timer* p;
  while (TI->size > 0) {
    p = &TI->data[HEAP0];
    if CMP_TIME(&(p->deadline), &cur_tv, <=) {
      outbuffer[n++] = p->session;
      if ((p->count==-1) || (--(p->count)>0)) {
        ADD_TIME(&(p->deadline), &(p->interval), &(p->deadline));
        downheap(TI->data, TI->size, HEAP0);
      }else{
        TI->data[HEAP0] = TI->data[TI->size--];
        if (TI->size > 1) {
          downheap(TI->data, TI->size, HEAP0);
        }
      }
      if (n >= maxsize) {
        break;
      }
    }else {
      break;
    }
  }
  return n;
}