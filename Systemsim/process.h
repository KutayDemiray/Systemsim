#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include "pcb_queue.h"

typedef struct {

} process_arg;

// simulated process as a thread
static void *process(void *arg) {

}

