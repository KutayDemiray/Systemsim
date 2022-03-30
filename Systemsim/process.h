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
	pthread_mutex_t *mutex_sim; // shared
	pid_list *pid_list; // shared
} process_arg;

// simulated process as a thread (there may be many) TODO work in progress
static void *process_th(void *args) {
	// read args
	cpu *cpu = ((process_arg) args)->cpu;
	pcb *pcb = ((process_arg) args)->pcb;
	pthread_cond_t *cv_sch = ((process_arg) args)->cv_sch;
	pthread_cond_t *cv_rq = ((process_arg) args)->cv_rq;
	pthread_mutex_t *mutex_sim = ((process_arg) args)->mutex_sim;
	cl_args* cl = ((process_arg) args)->cl;
	
	time_t t;
	srand((unsigned) time(&t));
	
	int calcburst = 1; // next burst shouldn't be calculated before current one finishes
	double p;
	do {
		pthread_mutex_lock(mutex_sim);
		
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
		int burstwidth = cl->max_burst - cl->min_burst;
		if (calcburst && cl->burst_dist != FIXED) {
			double u, next;
			if (cl->burst_dist == UNIFORM) {
				u = ((double) (rand() % 1000)) / 1000.0;
				next = (int) (burstwidth * u);
			}
			else if (cl->burst_dist == EXPONENTIAL) { // algorithm below is taken from prof. korpeoglu's forum post
				do {
					u = ((double) (rand() % 1000)) / 1000.0;
					next = (int) ((-1) * log(u) * cl->burst_len);
				} while (next < cl->min_burst || next > cl->max_burst)
			}
			else {
				next = cl->burst_len; // we need to do this in order to initialize the burst length
			}

			pcb->next_burst_len = next;
		}
		
		// "do" the burst by sleeping
		if (cl->alg == ALG_RR && cl->q < pcb->remaining_burst_len) {
			pcb->state = PCB_RUNNING;
			pcb->remaining_burst_len -= q;
			calcburst = 0;
			if (cl->outmode == 2){
				printf(); // I NEED HELP THERE HOW TO CHECK THE CURRENT TIME 
				printf("\t");
				printf(pcb->p_id);
				printf("\t");
				printf(pcb->state);
				printf("\n");
			}
			usleep(cl->q);
			pcb->state = PCB_READY;
			
			pthread_cond_signal(cv_sch); // wake up scheduler (case 3)
			// in this case, no i/o will happen. instead, the process will simply be added to the ready queue again
		}
		else {
			usleep(pcb->remaining_burst_len);
			calcburst = 1;
			int device_no = 0;
			// deal with i/o if needed
			if (p > cl->p0) {
				io_device *dev;
				int duration;
				pthread_mutex_t *dev_lock;
				if (cl->p0 <= p && p < cl->p0 + cl->p1) { // i/o with device 1
					dev = dev1;
					duration = cl->t1;
					dev_lock = dev1_lock;
					device_no = 1;
				}
				else if (cl->p0 + cl->p1 <= p) { // i/o with device 2
					dev = dev2;
					duration = cl->t2;
					dev_lock = dev2_lock;
					device_no = 2;
				}
				
				pcb->state = PCB_WAITING;

				pthread_mutex_lock(dev->mutex);
				
				dev->count++;
				while (dev->cur != NULL) {
					pthread_cond_wait(dev->cv, dev->mutex);
				}
				pthread_cond_signal(cv_sch); // wake up scheduler (case 2)
				
				dev->cur = pcb;
				
				//pthread_mutex_unlock(dev->mutex);
				if (cl->outmode == 2){
					printf(); // I NEED HELP THERE HOW TO CHECK THE CURRENT TIME 
					printf("\t");
					printf(pcb->p_id);
					printf("\t");
					printf("USING DEVICE %d", device_no);
					printf("\n");
				}

				usleep(duration);
				
				//pthread_mutex_lock(dev->mutex);
				
				dev->cur = NULL;
				dev->count--;
				
				pthread_cond_signal(dev->cv);
				pthread_mutex_unlock(dev->mutex);
				
				pcb->state = PCB_READY;
				enqueue(&(cpu->rq), pcb);
				pthread_cond_signal(cv_sch); // wake up scheduler (case 4)
			}
		}
		
		pthread_mutex_unlock(mutex_sim);
	} while (p > cl->p0);
	
	pthread_cond_signal(cv_sch); // wake up scheduler (case 1)
	return_pid(&(args->pid_list), pcb->pid);
	pthread_exit(NULL);
}

