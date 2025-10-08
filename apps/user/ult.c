/*
 * (C) 2023, Cornell University
 * All rights reserved.
 */

/* Author: Robbert van Renesse
 * Description: course project, user-level threading
 * Students implement a threading package and semaphore;
 * And then spawn multiple threads as either producer or consumer.
 */

#include "app.h"

/** These two functions are defined in grass/context.S **/
void ctx_start(void** old_sp, void* new_sp);
void ctx_switch(void** old_sp, void* new_sp);

/** Multi-threading functions **/

/* Global variables for threading system */
static struct thread *current_thread = NULL;
static struct thread *ready_queue = NULL;
static int thread_count = 0;

struct thread {
    /* Student's code goes here. */
    void *sp;                    /* Stack pointer */
    void (*func)(void *);        /* Thread function */
    void *arg;                   /* Thread argument */
    int state;                   /* Thread state: 0=ready, 1=running, 2=blocked */
    struct thread *next;         /* Next thread in queue */
};

void thread_init(){
    /* Student's code goes here */
    /* Initialize threading system */
    current_thread = NULL;
    ready_queue = NULL;
    thread_count = 0;
}

void ctx_entry(void){
    /* Student's code goes here. */
    /* Context entry point for new threads */
    if (current_thread && current_thread->func) {
        current_thread->func(current_thread->arg);
    }
    thread_exit();
}

void thread_create(void (*f)(void *), void *arg, unsigned int stack_size){
    /* Student's code goes here. */
    /* Allocate new thread structure */
    struct thread *new_thread = (struct thread*)malloc(sizeof(struct thread));
    if (!new_thread) return;
    
    /* Allocate stack */
    void *stack = malloc(stack_size);
    if (!stack) {
        free(new_thread);
        return;
    }
    
    /* Initialize thread */
    new_thread->sp = (char*)stack + stack_size - 8; /* Stack grows downward */
    new_thread->func = f;
    new_thread->arg = arg;
    new_thread->state = 0; /* Ready */
    new_thread->next = NULL;
    
    /* Add to ready queue */
    if (ready_queue == NULL) {
        ready_queue = new_thread;
    } else {
        struct thread *t = ready_queue;
        while (t->next) t = t->next;
        t->next = new_thread;
    }
    
    thread_count++;
}

void thread_yield(){
    /* Student's code goes here. */
    /* Save current thread and switch to next ready thread */
    if (current_thread && ready_queue) {
        /* Add current thread back to ready queue */
        current_thread->state = 0; /* Ready */
        current_thread->next = ready_queue;
        ready_queue = current_thread;
        
        /* Get next thread from ready queue */
        struct thread *next_thread = ready_queue;
        ready_queue = ready_queue->next;
        next_thread->next = NULL;
        next_thread->state = 1; /* Running */
        
        /* Switch context */
        struct thread *old_thread = current_thread;
        current_thread = next_thread;
        ctx_switch(&old_thread->sp, current_thread->sp);
    }
}

void thread_exit(){
    /* Student's code goes here. */
    /* Exit current thread and switch to next ready thread */
    if (current_thread) {
        thread_count--;
        
        /* Free thread resources */
        if (current_thread->sp) {
            /* Calculate stack base from stack pointer */
            void *stack_base = (char*)current_thread->sp - 16*1024 + 8;
            free(stack_base);
        }
        free(current_thread);
        current_thread = NULL;
        
        /* Switch to next ready thread */
        if (ready_queue) {
            struct thread *next_thread = ready_queue;
            ready_queue = ready_queue->next;
            next_thread->next = NULL;
            next_thread->state = 1; /* Running */
            current_thread = next_thread;
            ctx_start(NULL, current_thread->sp);
        }
    }
}

/** Semaphore functions **/

struct sema {
    /* Student's code goes here. */
    int count;                   /* Semaphore count */
    struct thread *wait_queue;   /* Queue of waiting threads */
};

void sema_init(struct sema *sema, unsigned int count){
    /* Student's code goes here. */
    sema->count = count;
    sema->wait_queue = NULL;
}

void sema_inc(struct sema *sema){
    /* Student's code goes here. */
    sema->count++;
    
    /* Wake up a waiting thread if any */
    if (sema->wait_queue) {
        struct thread *woken_thread = sema->wait_queue;
        sema->wait_queue = sema->wait_queue->next;
        woken_thread->next = NULL;
        woken_thread->state = 0; /* Ready */
        
        /* Add to ready queue */
        if (ready_queue == NULL) {
            ready_queue = woken_thread;
        } else {
            struct thread *t = ready_queue;
            while (t->next) t = t->next;
            t->next = woken_thread;
        }
    }
}

void sema_dec(struct sema *sema){
    /* Student's code goes here. */
    if (sema->count > 0) {
        sema->count--;
    } else {
        /* Block current thread */
        if (current_thread) {
            current_thread->state = 2; /* Blocked */
            current_thread->next = sema->wait_queue;
            sema->wait_queue = current_thread;
            
            /* Switch to next ready thread */
            if (ready_queue) {
                struct thread *next_thread = ready_queue;
                ready_queue = ready_queue->next;
                next_thread->next = NULL;
                next_thread->state = 1; /* Running */
                
                struct thread *old_thread = current_thread;
                current_thread = next_thread;
                ctx_switch(&old_thread->sp, current_thread->sp);
            }
        }
    }
}

int sema_release(struct sema *sema){
    /* Student's code goes here. */
    /* Release all waiting threads */
    int released_count = 0;
    while (sema->wait_queue) {
        struct thread *woken_thread = sema->wait_queue;
        sema->wait_queue = sema->wait_queue->next;
        woken_thread->next = NULL;
        woken_thread->state = 0; /* Ready */
        
        /* Add to ready queue */
        if (ready_queue == NULL) {
            ready_queue = woken_thread;
        } else {
            struct thread *t = ready_queue;
            while (t->next) t = t->next;
            t->next = woken_thread;
        }
        released_count++;
    }
    return released_count;
}

/** Producer and consumer functions **/

#define NSLOTS	3

static char *slots[NSLOTS];
static unsigned int in, out;
static struct sema s_empty, s_full;

static void producer(void *arg){
    for (;;) {
        // first make sure there's an empty slot.
        // then add an entry to the queue
        // lastly, signal consumers

        sema_dec(&s_empty);
        slots[in++] = arg;
        if (in == NSLOTS) in = 0;
        sema_inc(&s_full);
    }
}

static void consumer(void *arg){
    for (int i = 0; i < 5; i++) {
        // first make sure there's something in the buffer
        // then grab an entry to the queue
        // lastly, signal producers

        sema_dec(&s_full);
        void *x = slots[out++];
        if (out == NSLOTS) out = 0;
        printf("%s: got '%s'\n", arg, x);
        sema_inc(&s_empty);
    }
}

int main() {
    INFO("User-level threading implementation");

    thread_init();
    sema_init(&s_full, 0);
    sema_init(&s_empty, NSLOTS);

    thread_create(consumer, "consumer 1", 16 * 1024);
    thread_create(consumer, "consumer 2", 16 * 1024);
    thread_create(consumer, "consumer 3", 16 * 1024);
    thread_create(consumer, "consumer 4", 16 * 1024);
    thread_create(producer, "producer 2", 16 * 1024);
    thread_create(producer, "producer 3", 16 * 1024);
    producer("producer 1");
    thread_exit();

    return 0;
}

