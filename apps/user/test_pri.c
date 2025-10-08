#include "app.h"
#include "../grass/process.h"
#include <stdlib.h>

//once processes start up you can change to non defaults here

int main( int argc, char *argv[])
{
    if (argc != 3) {
        printf("Usage: %s <iterations> <priority>\n", argv[0]);
        return -1;
    }
    
    int iterationnumber = atoi(argv[1]);
    int prioritynumber = atoi(argv[2]);
    
    if (prioritynumber < 1 || prioritynumber > 10) {
        printf("Priority must be between 1 and 10\n");
        return -1;
    }
    
    if (iterationnumber <= 0) {
        printf("Iterations must be positive\n");
        return -1;
    }
    
    setpriority(getpid(), prioritynumber);
    printf("Process %d running %d iterations at priority %d\n", getpid(), iterationnumber, prioritynumber);
    
    /* Run the iterations */
    for (int i = 0; i < iterationnumber; i++) {
        /* Do some work to make the process consume CPU time */
        volatile int dummy = 0;
        for (int j = 0; j < 1000; j++) {
            dummy += j;
        }
        
        /* Print progress every 10 iterations */
        if ((i + 1) % 10 == 0) {
            printf("Process %d completed %d/%d iterations\n", getpid(), i + 1, iterationnumber);
        }
    }
    
    printf("Process %d completed all %d iterations\n", getpid(), iterationnumber);
}