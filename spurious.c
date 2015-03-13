#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>
#include <sched.h>

// CONFIG
const static unsigned int NUM_THREADS = 4;
// /CONFIG

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
volatile int available = 1;
unsigned spinLocked;
unsigned spinUnlocked;
volatile unsigned iterations;
volatile unsigned spurious;
volatile int stop = 0;

void *ThreadProc(void *arg)
{
  volatile unsigned count;

  // wait until all threads are started and we're ready to go
  while(!spinLocked && !spinUnlocked)
    sched_yield();

  while(!stop){
    pthread_mutex_lock(&mutex);
    while(!available){
      pthread_cond_wait(&cond, &mutex);
      // if the resource is not available the wakeup was spurious
      if(!available)
        ++spurious;
    }
    // Consume the resource and update the benchmark
    available--;
    iterations++;
    pthread_mutex_unlock(&mutex);

    // Pretend we're doing work for a while
    count = spinLocked;
    while(count)
      --count;

    // We're done doing our "work," so unlock and "free" the resource
    pthread_mutex_lock(&mutex);
    available++;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);

    // Do "other" work without a shared resource
    count = spinUnlocked;
    while(count)
      --count;
  }
  return NULL;
}

// alarm signal handler - this is where the count and duty cycle is updated
void SignalSIGALRM(int signum)
{
    static unsigned count = 100000;
    signal(SIGALRM, SignalSIGALRM);
    if(spinLocked || spinUnlocked){
        printf("%6u iterations %6u spurious\n", iterations, spurious);
        if(spinLocked == count){
            count /= 10;
            if(count < 1000)
                exit(0);
            spinLocked = 0;
        } else{
            spinLocked += count * 25 / 100;
        }
        spinUnlocked = count - spinLocked;
    } else{
        spinUnlocked = count;
    }
    iterations = 0;
    spurious = 0;
    printf("locked %6d unlocked %6d loop: ", spinLocked, spinUnlocked);
}

int main(int argn, char *argv[], char *envp[])
{
  pthread_t threads[NUM_THREADS];
  int i;
  struct itimerval itimervalue;
  int error;
  void (*savedSIGALRM)(int);

  // disable stdout buffering
  setvbuf(stdout, NULL, _IONBF, 0);

  savedSIGALRM = signal(SIGALRM, SignalSIGALRM);
  if(savedSIGALRM == SIG_ERR)
    perror("signal");

  // initialize to zero to have threads wait after startup
  spinLocked = 0;
  spinUnlocked = 0;

  printf("Starting up %d threads...\n", (int)(sizeof(threads)/sizeof(threads[0])));
  for(i=0; i<sizeof(threads)/sizeof(threads[0]); i++){
    error = pthread_create(threads+i, NULL, ThreadProc, NULL);
    if(error)
      printf("pthread_create returned %d\n", error);
  }

  itimervalue.it_interval.tv_sec = 0;
  itimervalue.it_interval.tv_usec = 100000;
  itimervalue.it_value.tv_sec = 0;
  itimervalue.it_value.tv_usec = 1;
  error = setitimer(ITIMER_REAL, &itimervalue, NULL);
  if(error)
    perror("setitimer");

  // main thread will just busy wait
  while(1)
    sched_yield();

  for(i=0; i<sizeof(threads)/sizeof(threads[0]); i++)
    pthread_cancel(threads[i]);

  return EXIT_SUCCESS;
}
