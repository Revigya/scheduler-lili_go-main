/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: scheduler and inter-process communication
 */


#include "egos.h"
#include "process.h"
#include "syscall.h"
#include <string.h>
#include <stdlib.h>
#define INTR_ID_SOFT       3
#define INTR_ID_TIMER      7

static void proc_yield();
static void proc_syscall();
static void (*kernel_entry)();

int proc_curr_idx;
struct process proc_set[MAX_NPROCESS];

void intr_entry(int id) {
    if (id == INTR_ID_TIMER && curr_pid < GPID_SHELL) {
        /* Do not interrupt kernel processes since IO can be stateful */
        timer_reset();
        return;
    }

    if (curr_pid >= GPID_USER_START && earth->tty_intr()) {
        /* User process killed by ctrl+c interrupt */
        INFO("process %d killed by interrupt", curr_pid);
        asm("csrw mepc, %0" ::"r"(0x800500C));
        return;
    }

    if (id == INTR_ID_SOFT)
        kernel_entry = proc_syscall;
    else if (id == INTR_ID_TIMER)
        kernel_entry = proc_yield;
    else
        FATAL("intr_entry: got unknown interrupt %d", id);

    /* Switch to the kernel stack */
    ctx_start(&proc_set[proc_curr_idx].sp, (void*)GRASS_STACK_TOP);
}

void ctx_entry() {
    /* Now on the kernel stack */
    int mepc, tmp;
    asm("csrr %0, mepc" : "=r"(mepc));
    proc_set[proc_curr_idx].mepc = (void*) mepc;

    /* Student's code goes here (page table translation). */
    /* Save the interrupt stack */
    /* Save current stack pointer to process control block */
    asm("csrr %0, sp" : "=r"(proc_set[proc_curr_idx].sp));
    /* Student's code ends here. */

    /* kernel_entry() is either proc_yield() or proc_syscall() */
    kernel_entry();

    /* Student's code goes here (page table translation). */
    /* Restore the interrupt stack */
    /* Restore stack pointer from process control block */
    asm("csrw sp, %0" ::"r"(proc_set[proc_curr_idx].sp));
    /* Student's code ends here. */

    /* Switch back to the user application stack */
    mepc = (int)proc_set[proc_curr_idx].mepc;
    asm("csrw mepc, %0" ::"r"(mepc));
    ctx_switch((void**)&tmp, proc_set[proc_curr_idx].sp);
}

static void proc_yield() {
    /* Student's code goes here (priority scheduler) */
    /* Multi-queue priority scheduler with aging implementation */
    static int priority_queues[11][MAX_NPROCESS]; /* 10 priority levels, 1-indexed */
    static int queue_counts[11] = {0};
    static int queue_heads[11] = {0};
    static int queue_tails[11] = {0};

    /* Initialize priority queues on first call */
    static int initialized = 0;
    if (!initialized) {
        for (int i = 0; i < 11; i++) {
            queue_counts[i] = 0;
            queue_heads[i] = 0;
            queue_tails[i] = 0;
            for (int j = 0; j < MAX_NPROCESS; j++) {
                priority_queues[i][j] = -1;
            }
        }
        initialized = 1;
    }

    int next_idx = -1;

    /* Check if aging is needed - when all higher priority queues are empty */
    int all_higher_empty = 1;
    for (int p = 1; p <= 9; p++) {
        if (queue_counts[p] > 0) {
            all_higher_empty = 0;
            break;
        }
    }

    /* Implement aging: promote all lower priority processes when higher ones are empty */
    if (all_higher_empty && queue_counts[10] > 0) {
        for (int p = 10; p > 1; p--) {
            if (queue_counts[p] > 0) {
                /* Move all processes from priority p to p-1 */
                for (int i = 0; i < queue_counts[p]; i++) {
                    int idx = (queue_heads[p] + i) % MAX_NPROCESS;
                    int pid = priority_queues[p][idx];
                    /* Find process and update current aged priority */
                    for (int j = 0; j < MAX_NPROCESS; j++) {
                        if (proc_set[j].pid == pid) {
                            proc_set[j].currentage = p - 1;
                            break;
                        }
                    }
                    /* Add to higher priority queue */
                    priority_queues[p-1][queue_tails[p-1]] = pid;
                    queue_tails[p-1] = (queue_tails[p-1] + 1) % MAX_NPROCESS;
                    queue_counts[p-1]++;
                }
                queue_counts[p] = 0;
                queue_heads[p] = 0;
                queue_tails[p] = 0;
            }
        }
    }

    /* Find highest priority process from queues */
    for (int p = 1; p <= 10; p++) {
        if (queue_counts[p] > 0) {
            int pid = priority_queues[p][queue_heads[p]];
            queue_counts[p]--;
            queue_heads[p] = (queue_heads[p] + 1) % MAX_NPROCESS;

            /* Find the actual process index */
            for (int i = 0; i < MAX_NPROCESS; i++) {
                if (proc_set[i].pid == pid) {
                    next_idx = i;
                    break;
                }
            }
            break;
        }
    }

    /* Fallback to round robin if no processes in priority queues */
    if (next_idx == -1) {
        for (int i = 1; i <= MAX_NPROCESS; i++) {
            int s = proc_set[(proc_curr_idx + i) % MAX_NPROCESS].status;
            if (s == PROC_READY || s == PROC_RUNNING || s == PROC_RUNNABLE) {
                next_idx = (proc_curr_idx + i) % MAX_NPROCESS;
                break;
            }
        }
    }

    /* Increment context switch counter for the process that will run */
    if (next_idx != -1) {
        proc_set[next_idx].ctx++;
    }

    /* Re-enqueue current process if it's running and a user process */
    if (curr_status == PROC_RUNNING && curr_pid >= GPID_USER_START) {
        /* Restore original priority after running */
        for (int i = 0; i < MAX_NPROCESS; i++) {
            if (proc_set[i].pid == curr_pid) {
                if (proc_set[i].currentage != proc_set[i].priority) {
                    proc_set[i].currentage = proc_set[i].priority;
                }
                /* Add to priority queue with current aged priority */
                int priority = proc_set[i].currentage;
                priority_queues[priority][queue_tails[priority]] = curr_pid;
                queue_tails[priority] = (queue_tails[priority] + 1) % MAX_NPROCESS;
                queue_counts[priority]++;
                break;
            }
        }
    }

    if (next_idx == -1) FATAL("proc_yield: no runnable process");
    if (curr_status == PROC_RUNNING) proc_set_runnable(curr_pid);
    /* Student's code ends here */

    /* Switch to the next runnable process and reset timer */
    proc_curr_idx = next_idx;
    earth->mmu_switch(curr_pid);
    timer_reset();

    /* Student's code goes here (switch privilege level). */

    /* Modify mstatus.MPP to enter machine or user mode during mret
     * depending on whether curr_pid is a grass server or a user app
     */
    unsigned int mstatus;
    asm volatile("csrr %0, mstatus" : "=r"(mstatus));
    if (curr_pid < GPID_USER) {
        // Grass server: set MPP to machine mode (11)
        mstatus = (mstatus & ~(3 << 11)) | (3 << 11);
    } else {
        // User app: set MPP to user mode (00)
        mstatus = (mstatus & ~(3 << 11));
    }
    asm volatile("csrw mstatus, %0" :: "r"(mstatus));

    /* Student's code ends here. */

    /* Call the entry point for newly created process */
    if (curr_status == PROC_READY) {
        proc_set_running(curr_pid);
        /* Prepare argc and argv */
        asm("mv a0, %0" ::"r"(APPS_ARG));
        asm("mv a1, %0" ::"r"(APPS_ARG + 4));
        /* Enter application code entry using mret */
        asm("csrw mepc, %0" ::"r"(APPS_ENTRY));
        asm("mret");
    }

    proc_set_running(curr_pid);
}

static void proc_send(struct syscall *sc) {
    sc->msg.sender = curr_pid;
    int receiver = sc->msg.receiver;

    for (int i = 0; i < MAX_NPROCESS; i++)
        if (proc_set[i].pid == receiver) {
            /* Find the receiver */
            if (proc_set[i].status != PROC_WAIT_TO_RECV) {
                curr_status = PROC_WAIT_TO_SEND;
                proc_set[proc_curr_idx].receiver_pid = receiver;
            } else {
                /* Copy message from sender to kernel stack */
                struct sys_msg tmp;
                earth->mmu_switch(curr_pid);
                memcpy(&tmp, &sc->msg, sizeof(tmp));

                /* Copy message from kernel stack to receiver */
                earth->mmu_switch(receiver);
                memcpy(&sc->msg, &tmp, sizeof(tmp));

                /* Set receiver process as runnable */
                proc_set_runnable(receiver);
            }
            proc_yield();
            return;
        }

    sc->retval = -1;
}

static void proc_recv(struct syscall *sc) {
    int sender = -1;
    for (int i = 0; i < MAX_NPROCESS; i++)
        if (proc_set[i].status == PROC_WAIT_TO_SEND &&
            proc_set[i].receiver_pid == curr_pid)
            sender = proc_set[i].pid;

    if (sender == -1) {
        curr_status = PROC_WAIT_TO_RECV;
    } else {
        /* Copy message from sender to kernel stack */
        struct sys_msg tmp;
        earth->mmu_switch(sender);
        memcpy(&tmp, &sc->msg, sizeof(tmp));

        /* Copy message from kernel stack to receiver */
        earth->mmu_switch(curr_pid);
        memcpy(&sc->msg, &tmp, sizeof(tmp));

        /* Set sender process as runnable */
        proc_set_runnable(sender);
    }

    proc_yield();
}

static void proc_syscall() {
    struct syscall *sc = (struct syscall*)SYSCALL_ARG;

    int type = sc->type;
    sc->retval = 0;
    sc->type = SYS_UNUSED;
    *((int*)0x2000000) = 0;

    switch (type) {
    case SYS_RECV:
        proc_recv(sc);
        break;
    case SYS_SEND:
        proc_send(sc);
        break;
    case SYS_SETPRIO:
        {
            int pid = sc->setprio_pid;
            int priority = sc->setprio_priority;

            if (priority < 1 || priority > 10) {
                sc->retval = -1;
                return;
            }

            for (int i = 0; i < MAX_NPROCESS; i++) {
                if (proc_set[i].pid == pid) {
                    proc_set[i].priority = priority;
                    proc_set[i].currentage = priority;
                    sc->retval = 0;
                    return;
                }
            }

            sc->retval = -1;
        }
        break;
    default:
        FATAL("proc_syscall: got unknown syscall type=%d", type);
    }
}
