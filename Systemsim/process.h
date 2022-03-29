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
	cl_args *cl; // shared
	cpu *cpu; // shared
	io_device *dev1, *dev2; // shared
	pcb *pcb; // shared
	pthread_cond_t *cv_sch; // shared
	pthread_cond_t *cv_rq; // shared
	sem_t *sem_mutex_sim; // shared
} process_arg;

// simulated process as a thread (there may be many) TODO work in progress
static void *process_th(void *args) {
	// read args
	cpu *cpu = ((process_arg) args)->cpu;
	pcb *pcb = ((process_arg) args)->pcb;
	pthread_cond_t *cv_sch = ((process_arg) args)->cv_sch;
	pthread_cond_t *cv_rq = ((process_arg) args)->cv_rq;
	sem_t *sem_mutex_sim = ((process_arg) args)->sem_mutex_sim;
	
	time_t t;
	srand((unsigned) time(&t));
	
	int calcburst = 1; // next burst shouldn't be calculated before current one finishes
	double p;
	do {
		sem_wait(sem_mutex_sim);
		
		// wait until cpu scheduler wakes processes up
		// cpu scheduler wakes up all processes when it picks one to run
		// so all processes should check whether they're the running process or not when they are awake
		while (cpu->cur->pcb->p_id != pcb->p_id) {
			pthread_cond_wait(cv_rq);
		}
		
		// determine whether the process will i/o with a device or terminate after this burst
		int tmp = rand();
		p = ((double) (tmp % 1000)) / 1000.0;
		
		// determine next cpu burst length
		if (calcburst && cl->burst-dist != FIXED) {
			double u, next;
			if (cl->burst_dist == UNIFORM) {
				u = ((double) (rand() % 1000)) / 1000.0;
				next = (int) (burstlen * u);
			}
			else if (cl->burst_dist == EXPONENTIAL {
				next = -1;
				while (next < min_burst || next > max_burst) {
					double u = ((double) (rand() % 1000)) / 1000.0;
					next = (int) ((-1) * log(u) * burstlen);
				}
			}
			// otherwise burst dist is fixed, no need to change anything
		}
		
		// "do" the burst by sleeping
		if (cl->alg == ALG_RR && cl->q < pcb->next_burst_len) {
			pcb->next_burst_len -= q;
			calcburst = 0;
			usleep(cl->q);
			// in this case, no i/o will happen. instead, the process will simply be added to the ready queue again
		}
		else {
			usleep(pcb->next_burst_len);
			calcburst = 1;
			
			// deal with i/o if needed
			if (p > p0) {
				io_device *dev;
				if (p0 <= p && p < p0 + p1) { // i/o with device 1
					dev = dev1;
				}
				else if (p0 + p1 <= p) { // i/o with device 2
					dev = dev2;
				}
				
				pcb->state = PCB_WAITING;
			}
			
		}
		
		// if the process won't be terminated, put it back on the queue
		if (p > p0) {
			enqueue(&(cpu->rq), pcb);
		}
		
		sem_post(sem_mutex_sim);
	} while (p > p0);
	
	// TODO more termination handling?
	
	pthread_cond_signal(cv_sch); // wake up scheduler (case 1)
	
	pthread_exit(NULL);
}

