#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ucontext.h>
#include "thread.h"
#include "interrupt.h"

enum STATE {
	DELETED,
	SUSPENDED,
	BLOCKED,
	RUNNING
};

/* This is the thread control block */
typedef struct thread {
	int tid;
	ucontext_t context;
	enum STATE state;
}thread;

typedef struct t_node {
	thread current;
	struct t_node *next;
}t_node;

struct main_queue {
	struct t_node * next;
	int total_threads;
	int list_threads[THREAD_MAX_THREADS];
};

/* thread starts by calling thread_stub. The arguments to thread_stub are the
 * thread_main() function, and one argument to the thread_main() function. */
void
thread_stub(void (*thread_main)(void *), void *arg)
{
	int enabled = interrupts_set(1);
	Tid ret;
	thread_main(arg); // call thread_main() function with arg
	interrupts_set(enabled);
	ret = thread_exit(THREAD_SELF);
	// we should only get here if we are the last thread. 
	assert(ret == THREAD_NONE);
	// all threads are done, so process should exit
	exit(0);
}
/*Global variables to be inserted here*/
struct main_queue deleted_queue, ready_queue;

void
thread_init(void)
{
	//initialize the all the queues
	deleted_queue.next = NULL;
	ready_queue.next = (t_node *)malloc(sizeof(t_node));
	
	//create a new thread
	ready_queue.next->current.tid = 0;
	ready_queue.next->current.state = RUNNING;
	ready_queue.total_threads = 1;
	ready_queue.list_threads[0] = 1;
	int i;
	for(i = 1; i < THREAD_MAX_THREADS; i++)
		ready_queue.list_threads[i] = 0;
	ready_queue.next->next = NULL;
}

Tid
thread_create(void (*fn) (void *), void *parg)
{	
	int enabled = interrupts_set(0);
	//check if you have reached the maximum number of threads
	if(ready_queue.total_threads == THREAD_MAX_THREADS){
		interrupts_set(enabled);
		return THREAD_NOMORE;
	}
	//create new thread and allocate memory to it
	t_node *t = (t_node *)malloc(sizeof(t_node));
	//get the context of the current thread a.k.a pc, registers and arguments
	getcontext(&(t->current.context));
	//change pc to thread_stub
	t->current.context.uc_mcontext.gregs[REG_RIP] = (unsigned long)thread_stub;

	char *dummy = (char *)malloc((THREAD_MIN_STACK + 8)*sizeof(char));
	if(!dummy) {
		interrupts_set(enabled);
		return THREAD_NOMEMORY;
	}

	//set the stack
	t->current.context.uc_stack.ss_sp = dummy;
	//check if the stack could be allocated
	
	//set the size of the stack to the one in the header file
	t->current.context.uc_stack.ss_size = THREAD_MIN_STACK;
	//stack stuff
	t->current.context.uc_mcontext.gregs[REG_RSP] = (unsigned long)dummy + THREAD_MIN_STACK + 8;
	//set the arguments
	t->current.context.uc_mcontext.gregs[REG_RDI] = (unsigned long)fn;
	t->current.context.uc_mcontext.gregs[REG_RSI] = (unsigned long)parg;

	//add thread to the end of the queue
	t_node *ptr = ready_queue.next;
	while(ptr->next)
		ptr = ptr->next;
	//point the last to the current thread
	ptr->next = t;	
	t->next = NULL;
	//find available tid
	int j;
	for(j = 0; j < THREAD_MAX_THREADS; j++) 
		if(ready_queue.list_threads[j] != 1)
			break;
	ready_queue.list_threads[j] = 1;
	t->current.tid = j;
	t->current.state = SUSPENDED;
	ready_queue.total_threads++;
	interrupts_set(enabled);
	return j;
}

Tid
thread_yield(Tid want_tid)
{
	int enabled = interrupts_set(0);
	int flag_set = 0;
			
	//check for what kind of yield you will
	//be performing
	if(want_tid == THREAD_ANY){
		t_node *ptr = ready_queue.next, *ptr2;
		if(!ptr->next){
			interrupts_set(enabled);
			return THREAD_NONE;}
		else {
			ptr2 = ptr;
			//go through the linked list and reach the end
			while(ptr2->next)
				ptr2 = ptr2->next;
			//make head point to next thread in the linked list
			ready_queue.next = ready_queue.next->next;
			//mark the currently running thread as running
			ready_queue.next->current.state = RUNNING;
			//mark the old thread as suspended
			ptr->current.state = SUSPENDED;
			//make last thread of linked list point to the last running thread
			ptr2->next = ptr;
			//set ptr->null
			ptr->next = NULL;
			//save tid of the next thread
			int thread_id = ready_queue.next->current.tid;
			//save the context of the current running thread
			getcontext(&(ptr->current.context));
			//check if the flag has been set
			
			if(flag_set == 0) {
				flag_set = 1;
				setcontext(&(ready_queue.next->current.context));
			}			
			else {
				flag_set = 0;
				interrupts_set(enabled);
				return thread_id;
			}

		}

	}
	//just return the current thread id
	else if(want_tid == THREAD_SELF){
		//just return the thread id of the current thread (head of linked list)
		int thread_id = ready_queue.next->current.tid;
		getcontext(&(ready_queue.next->current.context));
		
		if(flag_set == 0) {
			flag_set = 1;
			setcontext(&(ready_queue.next->current.context));
		}			
		else {
			flag_set = 0;
			interrupts_set(enabled);
			return thread_id;
		}
	}
	
	else {
		if(want_tid == ready_queue.next->current.tid) {
			getcontext(&(ready_queue.next->current.context));
 			goto a;
		}
		t_node *ptr = ready_queue.next, *ptr2, *ptr3, *ptr4;
		//ptr2 will hold the thread preceding the wanted thread
		ptr2 = ptr;
		//go through the linked list looking for the current tid
		while(ptr2->next && ptr2->next->current.tid != want_tid)
			ptr2 = ptr2->next;
		//check if the end has been reached
		if(!ptr2->next){
			interrupts_set(enabled);
			return THREAD_INVALID;}
		//move the wanted thread to the front
		ptr3 = ptr2;
		ptr2 = ptr2->next;
		ptr3->next = ptr3->next->next;
		ptr2->next = ready_queue.next->next;
		ready_queue.next = ptr2;
		//find the end of the linked list
		ptr4 = ready_queue.next;
		while(ptr4->next)
			ptr4 = ptr4->next;
		ptr4->next = ptr;
		ptr->next = NULL;
		//mark the currently running thread as running
		ready_queue.next->current.state = RUNNING;
		//mark the old thread as suspended
		ptr->current.state = SUSPENDED;
		//save tid of the next thread
		int thread_id = ready_queue.next->current.tid;
		//save the context of the current running thread
		getcontext(&(ptr->current.context));
		//check if the flag has been set
	a:
		if(flag_set == 0) {
			flag_set = 1;
			setcontext(&(ready_queue.next->current.context));
		}			
		else {
			flag_set = 0;
			interrupts_set(enabled);
			return thread_id;
		}

	}
	interrupts_set(enabled);
	return THREAD_FAILED;
}

Tid
thread_exit(Tid tid)
{	
	int enabled = interrupts_set(0);
	if(tid == THREAD_ANY) {
		//check if there is only thread running
		if(!ready_queue.next->next){
			interrupts_set(enabled);
			return THREAD_NONE;}
		//destroy the next thread
		t_node *ptr = ready_queue.next->next;
		int thread_id = ptr->current.tid;
		ready_queue.next->next = ready_queue.next->next->next;
		ready_queue.list_threads[thread_id] = 0;
		ready_queue.total_threads--;
		ptr->next = deleted_queue.next;
		deleted_queue.next = ptr;
		ptr->current.state = DELETED;
		interrupts_set(enabled);
		return thread_id;	
	}
	else if(tid == THREAD_SELF || tid == ready_queue.next->current.tid){
		//check if there is only thread running
		if(!ready_queue.next->next){
			interrupts_set(enabled);
			return THREAD_NONE;
		}
		//destroy the next thread
		t_node *ptr = ready_queue.next;
		int thread_id = ptr->current.tid;
		ready_queue.next = ready_queue.next->next;
		ready_queue.list_threads[thread_id] = 0;
		ready_queue.total_threads--;
		ptr->next = deleted_queue.next;
		deleted_queue.next = ptr;
		ptr->current.state = DELETED;
		setcontext(&(ready_queue.next->current.context));
	}
	else {
		//go for desired thread_id
		t_node *ptr = ready_queue.next, *ptr2;
		while(ptr->next && ptr->next->current.tid != tid)
			ptr = ptr->next;
		if(!ptr->next){
			interrupts_set(enabled);
			return THREAD_INVALID;
		}
		//get rid of thread to be deleted
		ptr2 = ptr->next;
		int thread_id = ptr2->current.tid;
		ptr->next = ptr->next->next;
		ready_queue.list_threads[thread_id] = 0;
		ready_queue.total_threads--;
		ptr2->next = deleted_queue.next;
		deleted_queue.next = ptr2;
		ptr2->current.state = DELETED;
		interrupts_set(enabled);
		return tid;
	}
	interrupts_set(enabled);
	return THREAD_FAILED;
}

Tid
thread_sleep(struct wait_queue *queue)
{
	int enabled = interrupts_set(0);
	int flag_set = 0;
			
	//check for what kind of yield you will
	//be performing
	if(!queue){
		interrupts_set(enabled);
		return THREAD_INVALID;
	}
	if(!ready_queue.next->next){
		interrupts_set(enabled);
		return THREAD_NONE;
	}		
	else {
		t_node *ptr = ready_queue.next, *ptr2;
		ready_queue.next = ready_queue.next->next;
		ready_queue.next->current.state = RUNNING;
		ptr->current.state = BLOCKED;
		ptr->next = NULL;
		ptr2 = queue->next;
		if(!ptr2)
			queue->next = ptr;
		else {
			while(ptr2->next)	
				ptr2 = ptr2->next;
			ptr2->next = ptr;

		}
		int id = ready_queue.next->current.tid;
		getcontext(&(ptr->current.context));
		if(flag_set == 0) {
			flag_set = 1;
			setcontext(&(ready_queue.next->current.context));
		}			
		else {
			flag_set = 0;
			interrupts_set(enabled);
			return id;
		}
	}
	interrupts_set(enabled);
	return THREAD_FAILED;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int
thread_wakeup(struct wait_queue *queue, int all)
{
	int enabled = interrupts_set(0);
	if(!queue || !queue->next)
		return 0;
	if(all == 0){
		//there is at least one thread in the wait queue
		t_node *ptr = queue->next, *ptr2, *ptr3;
		//change the order of the queue
		queue->next = queue->next->next;
		ptr->current.state = SUSPENDED;
		ptr->next = NULL;
		ptr2 = ready_queue.next;
		if(!ptr2->next)
			ready_queue.next->next = ptr;
		else {
			ptr3 = ready_queue.next;
			while(ptr3->next)
				ptr3 = ptr3->next;
			ptr3->next = ptr;
		}
		interrupts_set(enabled);
		return 1;	
	}
	else if(all == 1){
		if(!queue || !queue->next || !ready_queue.next)
			return 0;
		//there is at least one thread in the wait queue
		int threads = 0;
		while(queue->next) {
			t_node *ptr = queue->next, *ptr2, *ptr3;
			queue->next = queue->next->next;
			ptr->next = NULL;
			ptr->current.state = SUSPENDED;
			ptr2 = ready_queue.next;
			threads++;
			if(!ptr2->next)
				ready_queue.next->next = ptr;
			else {
				ptr3 = ready_queue.next;
				while(ptr3->next)
					ptr3 = ptr3->next;
				ptr3->next = ptr;
			}
		}
		interrupts_set(enabled);
		return threads;
	}
	interrupts_set(enabled);
	return 0;
}
//return 1 upon success and -1 otherwise
int thread_destroy(struct wait_queue *queue, int tid) {
    //disable interrupts
    int enabled = interrupts_set(0);
    //go for desired thread_id
    t_node *ptr = ready_queue.next, *ptr2;
    while(ptr->next && ptr->next->current.tid != tid)
        ptr = ptr->next;
    if(!ptr->next){
        interrupts_set(enabled);
        return THREAD_INVALID;
    }
    //deallocate memory
    ptr2 = ptr->next;
    ptr->next = ptr->next->next;
    if(ptr2->current.state == DELETED)
        free(ptr2);
    else return THREAD_INVALID;
}