#include "egos.h"

void setpriority( int pid, int setprio ) {
    /* Student's code goes here (implement setpriority system call) */
    struct syscall *sc = (struct syscall*)SYSCALL_ARG;
    sc->type = SYS_SETPRIO;
    sc->setprio_pid = pid;
    sc->setprio_priority = setprio;
    asm("ecall");
    /* Student's code ends here */
}