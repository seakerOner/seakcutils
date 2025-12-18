#include "threadpool.h"
#include "../channels/spmc.h"

typedef struct Worker_t {
  ReceiverSpmc *receiver;
  ChannelSpmc *chan_ref;
} Worker;

typedef struct ThreadPool_t {
  pthread_t *workers;
  size_t num_workers;

  ChannelSpmc *channel;
  SenderSpmc *dispatcher;
} ThreadPool;

ThreadPool *threadpool_init(size_t num_threads) {
  ThreadPool *tp = malloc(sizeof(ThreadPool));

  tp->workers = malloc(num_threads * sizeof(pthread_t));
  tp->num_workers = num_threads;

  tp->channel = channel_create_spmc(num_threads * 4, sizeof(__Job__));
  tp->dispatcher = spmc_get_sender(tp->channel);

  for (size_t i = 0; i < num_threads; i++) {
    Worker *worker = malloc(sizeof(Worker));
    worker->receiver = spmc_get_receiver(tp->channel);
    worker->chan_ref = tp->channel;

    pthread_create(&tp->workers[i], NULL, __set_worker, worker);
  }

  return tp;
}

void threadpool_execute(ThreadPool *threadpool, __job __func, void *arg) {
  __Job__ job;
  job.job = __func;
  job.arg = arg;

  spmc_send(threadpool->dispatcher, &job);
};

void threadpool_shutdown(ThreadPool *threadpool) {
  spmc_close(threadpool->channel);
  for (size_t x = 0; x < threadpool->num_workers; x++) {
    pthread_join(threadpool->workers[x], NULL);
  }
  spmc_destroy(threadpool->channel);
  free(threadpool->workers);
  free(threadpool->dispatcher);
  free(threadpool);
};

void *__set_worker(void *arg) {
  Worker *worker = (Worker *)arg;
  __Job__ job;
  while (1) {
    if (spmc_recv(worker->receiver, &job) == CHANNEL_OK) {
      job.job(job.arg);
    } else if (spmc_is_closed(worker->chan_ref) == CLOSED) {
      break;
    }
  }

  spmc_close_receiver(worker->receiver);
  free(worker->receiver);
  free(worker);
  return NULL;
};
