#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    struct thread_data* pData = (struct thread_data*)thread_param;
    // wait for mutex lock
    printf("wait for mutex lock\n");
    usleep(pData->wait_to_obtain_ms * 1000);
    // obtain mutex
    printf("obtain mutex\n");
    pthread_mutex_lock(pData->mutex);
    // wait for mutex release
    printf("wait for mutex release\n");
    usleep(pData->wait_to_release_ms * 1000);
    // release mutex
    printf("release mutex\n");
    pthread_mutex_unlock(pData->mutex);

    printf("Update complete status\n");
    pData->thread_complete_success = true;

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    // allocate memory for thread_data
    struct thread_data* thread_data = malloc(sizeof(struct thread_data));
    if (thread_data == NULL)
    {
        printf("Failed to allocate memory for thread_data\n");
        goto exit;
    }
    // setup mutex and wait arguments
    thread_data->mutex = mutex;
    thread_data->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_data->wait_to_release_ms = wait_to_release_ms;
    thread_data->thread_complete_success = false;
    // pass thread_data to created thread
    int threadId = pthread_create(thread, NULL, threadfunc, thread_data);
    if (threadId != 0)
    {
        printf("Failed to created thread\n");
        free(thread_data);
        goto exit;
    }
exit:
    return false;
}

