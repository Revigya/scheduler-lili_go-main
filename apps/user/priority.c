#include "app.h"
#include "../grass/process.h"
#include <stdlib.h>
//#include <stdio.h>

int main( int argc, char *argv[])
{

    int inputpid = atoi(argv[1]);
    int prionumber = atoi(argv[1]);
    setpriority(inputpid, prionumber);
    //PROC_SETPRIO;
}