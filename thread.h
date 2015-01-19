#ifndef _THREAD_H_
#define _THREAD_H_

typedef int Tid;
#define THREAD_MAX_THREADS 1024
#define THREAD_MIN_STACK 32768

/*variables needed for coordinating different thread errors inbetween states*/

enum { THREAD_ANY = -1,
	THREAD_SELF = -2,
	THREAD_INVALID = -3,
	THREAD_NONE = -4,
	THREAD_NOMORE = -5,
	THREAD_NOMEMORY = -6,
	THREAD_FAILED = -7
};

//tester function for deciding whether a thread is valid or not
static inline int
thread_ret_ok(Tid ret)
{
	return (ret >= 0 ? 1 : 0);
}
//function used for initializing thread (allocating memory and initializing first running thread)
void thread_init(void);
//creates more threads and puts them into ready queue
Tid thread_create(void (*fn) (void *), void *arg);
//switching context between threads according to scheduler
Tid thread_yield(Tid tid);
//marking a thread as being exited
Tid thread_exit(Tid tid);
//switching thread for running state to blocked
Tid thread_sleep(struct wait_queue *queue);
//switching thread from blocked state to ready state
int thread_wakeup(struct wait_queue *queue, int all);
//deallocating memory of chosen exited thread
int thread_destroy(struct wait_queue *queue, int tid);
#endif /* _THREAD_H_ */
