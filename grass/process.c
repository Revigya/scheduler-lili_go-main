/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: helper functions for managing processes
 */

#include "egos.h"
#include "process.h"
#include "syscall.h"
#include <string.h>

void intr_entry(int id);

void excp_entry(int id) {
    /* Student's code goes here (handle memory exception). */
    
    /* If id is for system call, handle the system call and return */
    /* Handle system call exception */
    if (id == 11) { /* Environment call from U-mode (system call) */
        /* System call handling is done in intr_entry */
        return;
    }
    
    /* Otherwise, kill the process if curr_pid is a user application */
    /* Kill user process on other exceptions */
    if (curr_pid >= GPID_USER_START) {
        INFO("Killing user process %d due to exception %d", curr_pid, id);
        grass->proc_free(curr_pid);
        return;
    }

    /* Student's code ends here. */

    FATAL("excp_entry: kernel got exception %d", id);
}

void proc_init() {
    earth->intr_register(intr_entry);
    earth->excp_register(excp_entry);

    /* Student's code goes here (PMP memory protection). */
    /* Setup PMP TOR region 0x00000000 - 0x20000000 as r/w/x */
    asm("csrw pmpaddr0, %0" ::"r"(0x20000000 >> 2));
    asm("csrw pmpcfg0, %0" ::"r"(0x0F)); /* TOR, R/W/X */
    
    /* Setup PMP NAPOT region 0x20400000 - 0x20800000 as r/-/x */
    asm("csrw pmpaddr1, %0" ::"r"((0x20400000 >> 2) | 0x3FF)); /* NAPOT for 1MB */
    asm("csrw pmpcfg1, %0" ::"r"(0x1B)); /* NAPOT, R/X */
    
    /* Setup PMP NAPOT region 0x20800000 - 0x20C00000 as r/-/- */
    asm("csrw pmpaddr2, %0" ::"r"((0x20800000 >> 2) | 0x3FF)); /* NAPOT for 1MB */
    asm("csrw pmpcfg2, %0" ::"r"(0x19)); /* NAPOT, R */
    
    /* Setup PMP NAPOT region 0x80000000 - 0x80004000 as r/w/- */
    asm("csrw pmpaddr3, %0" ::"r"((0x80000000 >> 2) | 0x1F)); /* NAPOT for 64KB */
    asm("csrw pmpcfg3, %0" ::"r"(0x1D)); /* NAPOT, R/W */
    /* Student's code ends here. */

    /* The first process is currently running */
    proc_set_running(proc_alloc());
}

static void proc_set_status(int pid, int status) {
    for (int i = 0; i < MAX_NPROCESS; i++)
        if (proc_set[i].pid == pid) proc_set[i].status = status;
}

int proc_alloc() {
    static int proc_nprocs = 0;
    for (int i = 0; i < MAX_NPROCESS; i++)
        if (proc_set[i].status == PROC_UNUSED) {
            proc_set[i].pid = ++proc_nprocs;
            proc_set[i].status = PROC_LOADING;
            proc_set[i].priority = 2;
            //proc_set[i].priority = priority;
            //insert into scheduler queue;


            return proc_nprocs;
        }

    FATAL("proc_alloc: reach the limit of %d processes", MAX_NPROCESS);
}

void proc_free(int pid) {
    if (pid != -1) {
        earth->mmu_free(pid);
        proc_set_status(pid, PROC_UNUSED);
        return;
    }

    /* Free all user applications */
    for (int i = 0; i < MAX_NPROCESS; i++)
        if (proc_set[i].pid >= GPID_USER_START &&
            proc_set[i].status != PROC_UNUSED) {
            earth->mmu_free(proc_set[i].pid);
            proc_set[i].status = PROC_UNUSED;
        }
}

void proc_set_ready(int pid) { proc_set_status(pid, PROC_READY); }
void proc_set_running(int pid) { proc_set_status(pid, PROC_RUNNING); }
void proc_set_runnable(int pid) { proc_set_status(pid, PROC_RUNNABLE); }
int  proc_get_pid( ){ return curr_pid; }
struct process * proc_get_proc_set( ){ return &proc_set[0];}
void proc_set_priority(int pid, int setprio)
{
    if(pid >= 0 && pid < MAX_NPROCESS)
    {
        if(setprio < 11)
        {
            proc_set[pid].priority = setprio;
        }
    }
    else
    {
        printf("ERROR: Invalid PID provided\n");
    }
}

//enum PRIORITY proc_get_priority(int pid)
int proc_get_priority(int pid)
{
    if(pid >= 0 && pid < MAX_NPROCESS)
    {
        //proc_set[pid].priority = setprio;
        return proc_set[pid].priority;
    }
    else
    {
        printf("ERROR: Invalid PID provided\n");
        return 10;
    }
}