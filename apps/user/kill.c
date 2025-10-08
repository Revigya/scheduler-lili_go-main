#include "app.h"
#include "../grass/process.h"
#include <stdlib.h>

int main(int argc, char *argv[])
{
    
    printf("Killing Process %s ...\n", argv[1]);
    int inputpid = atoi(argv[1]);
    grass->proc_free(inputpid);

}