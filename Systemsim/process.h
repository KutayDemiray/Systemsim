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
	pthread_mutex_t *mutex_sim; // shared
	pid_list **pid_list; // shared
} process_arg;

// simulated process as a thread (there may be many) TODO work in progress
static void *process_th(void *args) {
	// read args
	cpu *cpu = ((process_arg) args)->cpu;
	pcb *pcb = ((process_arg) args)->pcb;
	pthread_cond_t *cv_sch = ((process_arg) args)->cv_sch;
	pthread_cond_t *cv_rq = ((process_arg) args)->cpu->rq->cv;
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
		if (pcb->remaining_burst_time == 0 && cl->burst_dist != FIXED) {
			double u, next;
			if (cl->burst_dist == UNIFORM) {
				u = ((double) (rand() % 1000)) / 1000.0;
				next = (int) (burstwidth * u);
			}
			else if (cl->burst_dist == EXPONENTIAL) { // exponential distribution
				// algorithm below is taken from prof. korpeoglu's forum post
				// exponential distribution with lambda = 1/burst_len
				do {
					u = ((double) (rand() % 1000)) / 1000.0;
					next = (int) ((-1) * log(u) * cl->burst_len);
				} while (next < cl->min_burst || next > cl->max_burst)
			}
			else { // fixed
				next = cl->burst_len;
			}

			pcb->next_burst_len = next;
		}
		
		// "do" the burst by sleeping
		if (cl->alg == ALG_RR && cl->q < pcb->remaining_burst_len) {
			pcb->state = PCB_RUNNING;
			usleep(cl->q * 1000);
			pcb->remaining_burst_len -= cl->q;
			pcb->total_time += cl->q;
			pcb->state = PCB_READY;
			
			if (cl->outmode == 2){
				printf("%d\t%d\t", (int) (gettimeofday(&t, NULL) - start_time), pcb->p_id);
				case (pcb->state)
					1: printf("RUNNING\n");
					2: printf("WAITING\n");
					3: printf("READY\n")
			}

			pthread_cond_signal(cv_sch); // wake up scheduler (case 3)
			// in this case, no i/o will happen. instead, the process will simply be added to the ready queue again
		}
		else {
			usleep(pcb->remaining_burst_len * 1000);
			pcb->total_time += pcb->remaining_burst_len;
			pcb->remaining_burst_len = 0;

			// deal with i/o if needed
			if (p > cl->p0) {
				io_device *dev;
				int duration;
				pthread_mutex_t *dev_lock;
				int device_no = 0;
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
				if (pl->outmode == 2) {
					printf("%d\t"(int) (gettimeofday(&t, NULL) - start_time));
					printf("%d\t", pcb->p_id);
					printf("USING DEVICE %d\n", device_no);
				}

				usleep(duration * 1000);
				
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
	return_pid(args->pid_list, pcb->pid);
	pthread_exit(NULL);
	
}

