#include "app.h"
#include "../grass/process.h"

int main(int argc, char** argv) {

    int i;
    struct process * process_table = grass->proc_get_proc_set();
    
    printf("PID\tSTATUS\tPRIORITY\tCTX\n");
    for( i = 0; i < MAX_NPROCESS; i++ )
    {
      if( process_table[i].pid )
      {
        printf("%d\t%d\t%d\t\t%d\n", process_table[i].pid, process_table[i].status, process_table[i].priority, process_table[i].ctx);
        //printf("%d\t%d\t%d\t\t%d\n", process_table[i].pid, process_table[i].status, getpriority(i), process_table[i].ctx);
      
      }
    }

    return 0;
}