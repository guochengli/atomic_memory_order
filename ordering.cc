#include <atomic>
#include <pthread.h>
#include <thread>
#include <semaphore.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>

// Set either of these to 1 to prevent CPU reordering
#define USE_SINGLE_HW_THREAD       0  // Supported on Linux, but not Cygwin or PS3

#if USE_SINGLE_HW_THREAD
#include <sched.h>
#endif

//sem_t beginSema1, beginSema2, endSema;
std::atomic_int volatile begin1, begin2, end;
std::atomic_int X, Y;
std::atomic_int r1, r2;

void thread1Func() {
  for (;;) {
    //sem_wait(&beginSema1);  // Wait for signal
    while (begin1 == 0); begin1 = 0;
    while (std::rand() % 8 != 0) {}  // Random delay
    X.store(1, std::memory_order_relaxed);  // if changed to seq_cst, no data race
    //std::atomic_thread_fence(std::memory_order_seq_cst);
    r1.store(Y.load(std::memory_order_relaxed), std::memory_order_seq_cst);
    //sem_post(&endSema);  // Notify transaction complete
    end++;
  }
};

void thread2Func() {
  for (;;) {
    //sem_wait(&beginSema2);  // Wait for signal
    while (begin2 == 0); begin2 = 0;
    while (std::rand() % 8 != 0) {}  // Random delay
    Y.store(1, std::memory_order_relaxed);  // if changed to seq_cst, no data race
    //std::atomic_thread_fence(std::memory_order_seq_cst);
    r2.store(X.load(std::memory_order_relaxed), std::memory_order_seq_cst);
    //sem_post(&endSema);  // Notify transaction complete
    end++;
  }
};

int main() {
  std::srand(std::time(nullptr));
  //sem_init(&beginSema1, 0, 0);
  //sem_init(&beginSema2, 0, 0);
  //sem_init(&endSema, 0, 0);
  std::thread thread1(&thread1Func), thread2(&thread2Func);

#if USE_SINGLE_HW_THREAD
  if (std::thread::hardware_concurrency() > 1) {
    // Force thread affinities to the same cpu core.
    cpu_set_t cpus;
    CPU_ZERO(&cpus);
    CPU_SET(0, &cpus);
    pthread_setaffinity_np(thread1.native_handle(), sizeof(cpu_set_t), &cpus);
    pthread_setaffinity_np(thread2.native_handle(), sizeof(cpu_set_t), &cpus);
  }
  else {
    std::cout << "std::thread::hardware_concurrency() = " << std::thread::hardware_concurrency() << std::endl;
  }
#endif

  int detected = 0;
  for (int iterations = 1; ; iterations++) {
    X = 0;
    Y = 0;
    //sem_post(&beginSema1);
    //sem_post(&beginSema2);
    begin1 = 1;
    begin2 = 1;
    while (end < 2); end = 0;
    //sem_wait(&endSema);
    //sem_wait(&endSema);
    if (r1 == 0 && r2 == 0) {
      detected++;
      printf("%d reorders detected after %d iterations\n", detected, iterations);
    }
  }
  return 0;  // Never returns
}

