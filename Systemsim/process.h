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
	// TODO possibly a pointer to the current process in the cpu
	// TODO also need to access scheduler cv
} process_arg;

// simulated process as a thread (there may be many)
static void *process_th(void *args) {
	// TODO wait until cpu scheduler wakes all processes up
	
	// TODO cpu scheduler wakes up all processes when it picks one to run
	// so all processes should check whether they're the ready process or not when they run
	
	// TODO wake up scheduler
	
	pthread_exit(NULL);
}

