#include "dmlogger.h"
#include <time.h>

#define TEST_THREADS 16          // Númber of threads used in test.
#define TEST_LOGS    200000     // Númber of logs per thread.
#define TEST_QUEUE_BUFFER 4000000   // Buffer length.
#define TEST_QUEUE_POLICY DMLOGGER_OFPOLICY_WAIT_TIMEOUT // Policy of queue overflow.

// Test thread that will log a bunch of logs:
void * test_th(void * arg)
{
    // Logger reference retreive:
    dmlogger_pt logger = (dmlogger_pt)arg;
    if (!logger) return NULL;
 
    // Loop to log TEST_LOGS logs within this thread:
    for (int i = 0; i < TEST_LOGS; i++) {
        dmlogger_log(logger, DMLOGGER_LEVEL_DEBUG, "Test message %d", i);
    }
    return NULL;
}

int main(int argc, char ** argv){
    // Logger variable:
    dmlogger_pt logger;
    
    // Logger initialization:
    dmlogger_init(&logger);
    if (!logger) goto error;

    // Logger run:
    if(!dmlogger_run(logger)) goto error;

    // Logger configurations:
    if(!dmlogger_conf_output_file(logger, "./logs/", "log", false, false, 0)) goto error;
    if(!dmlogger_conf_queue_ofpolicy(logger, TEST_QUEUE_POLICY, 2)) goto error;
    if(!dmlogger_conf_queue_capacity(logger, TEST_QUEUE_BUFFER)) goto error;
    if(!dmlogger_conf_logger_minlvl(logger, DMLOGGER_LEVEL_DEBUG)) goto error;

    // Logger testing:
    printf("Performance test:\n");

    pthread_t threads[TEST_THREADS];

    struct timespec t1, t2;
    clock_gettime(CLOCK_MONOTONIC, &t1);

    for (int i = 0; i < TEST_THREADS; i++) {
        pthread_create(&threads[i], NULL, test_th, logger);
    }
    for (int i = 0; i < TEST_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Wait until complete flush:
    dmlogger_flush(logger);
    clock_gettime(CLOCK_MONOTONIC, &t2);

    double t = (t2.tv_sec - t1.tv_sec) + (t2.tv_nsec - t1.tv_nsec) / 1e9;

    unsigned long total_logs = (unsigned long)TEST_THREADS * TEST_LOGS;

    double throughput = total_logs / t;

    printf("--------------------------------\n");
    printf("Threads: %d\n", TEST_THREADS);
    printf("Total logs: %lu\n", total_logs);
    printf("Total time: %.3f sec\n", t);
    printf("Throughput: %.0f logs/sec\n", throughput);
    printf("--------------------------------\n");

    return 0;
    
error:
    if (logger) dmlogger_deinit(&logger);
    return 1;
}