#ifndef PROCESS_H
#define PROCESS_H

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <pthread.h>
#include <math.h>
#include "pcb_queue.h"
#include "system.h"
#include <semaphore.h>

typedef struct {
	cl_args *cl; // shared
	cpu *cpu; // shared
	io_device *dev1, *dev2; // shared
	pcb *pcb; // shared
	pthread_cond_t *cv_sch; // shared
	pthread_mutex_t *mutex_sim; // shared
	pid_list **pid_list; // shared
	struct timeval start_time;
	sem_t *emptyprocs;
	int *should_schedule;
} process_arg;

int totalreturns = 0;

// simulated process as a thread (there may be many) TODO work in progress
static void *process_th(void *args) {
	// read args
	pthread_mutex_t *mutex_sim = ((process_arg *) args)->mutex_sim;
	pthread_mutex_lock(mutex_sim);
	cpu *cpu = ((process_arg *) args)->cpu;
	io_device *dev1 = ((process_arg *) args)->dev1;
	io_device *dev2 = ((process_arg *) args)->dev2;
	pcb *pcb = ((process_arg *) args)->pcb;
	pthread_cond_t *cv_sch = ((process_arg *) args)->cv_sch;
	pthread_cond_t cv_rq = ((process_arg *) args)->cpu->rq->cv;
	
	cl_args* cl = ((process_arg *) args)->cl;
	struct timeval start_time = ((process_arg *) args)->start_time;
	int *should_schedule = ((process_arg *) args)->should_schedule;
	
	//time_t t;
	//srand((unsigned) time(&t));
	
	double p;
	
	pcb->state = PCB_READY;
	enqueue(cpu->rq, pcb);
	*should_schedule = 1; // case 1
	pthread_cond_signal(cv_sch);
	pthread_mutex_unlock(mutex_sim);
	
	do {
		pthread_mutex_lock(mutex_sim);
		
		// 1. check if this process is the one selected by the scheduler 
		
		// wait until cpu scheduler wakes processes up
		// cpu scheduler wakes up all processes when it picks one to run
		// so all processes should check whether they're the running process or not when they are awake
		//while (cpu->cur != NULL && cpu->cur->p_id != pcb->p_id) {
		while (pcb->state != PCB_RUNNING) {
			pthread_cond_wait(&cv_rq, mutex_sim);
		}
		
		// 2. simulate burst
		
		// determine whether the process will i/o with a device or terminate after this burst
		int tmp = rand();
		p = ((double) (tmp % 1000)) / 1000.0;
		if (cl->outmode >= OUTMODE_VERBOSE) {
			printf("p: %f\n", p);
		}
		// determine next cpu burst length
		int burstwidth = cl->max_burst - cl->min_burst;
		if (pcb->remaining_burst_len == 0) {
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
				} while (next < cl->min_burst || next > cl->max_burst);
			}
			else { // fixed
				next = cl->burst_len;
			}

			pcb->next_burst_len = next;
			pcb->remaining_burst_len = next;
		} 
		
		// "do" the burst by sleeping
		dequeue(cpu->rq);
		if (cl->alg == ALG_RR && cl->q < pcb->remaining_burst_len) {
			//dequeue(cpu->rq);
			
			if (cl->outmode >= OUTMODE_VERBOSE) {
				printf("Process %d bursts for %d ms\n", pcb->p_id, cl->q);
			}
			//pthread_mutex_unlock(mutex_sim);
			usleep(cl->q * 1000);
			//pthread_mutex_lock(mutex_sim);
			cpu->cur = NULL;
			pcb->remaining_burst_len -= cl->q;
			pcb->total_time += cl->q;
			pcb->state = PCB_READY;
			
			if (cl->outmode >= OUTMODE_VERBOSE){
				struct timeval now;
				gettimeofday(&now, NULL);
				long int dif =  (now.tv_sec - start_time.tv_sec) * (1000) + (now.tv_usec - start_time.tv_usec) / (1000); 
				printf("%ld\t%d\t", dif, pcb->p_id);
				switch (pcb->state) {
					case PCB_RUNNING: printf("RUNNING\n"); break;
					case PCB_WAITING: printf("WAITING\n"); break;
					case PCB_READY: printf("READY\n"); break;
				}
			}
			
			// in this case, no i/o will happen. instead, the process will simply be added to the ready queue again
		}
		else {
			//pthread_mutex_unlock(mutex_sim);
			if (cl->outmode >= OUTMODE_VERBOSE) {
				printf("Process %d bursts for %d ms\n", pcb->p_id, pcb->remaining_burst_len);
			}
			
			usleep(pcb->remaining_burst_len * 1000);
			//pthread_mutex_lock(mutex_sim);
			
			cpu->cur = NULL;
			pcb->total_time += pcb->remaining_burst_len;
			pcb->remaining_burst_len = 0;
			pcb->state = PCB_READY;

			// deal with i/o if needed
			if (p > cl->p0) {
				io_device *dev;
				int duration;
				int device_no = 0;
				if (cl->p0 <= p && p < cl->p0 + cl->p1) { // i/o with device 1
					dev = dev1;
					duration = cl->t1;
					device_no = 1;
				}
				else if (cl->p0 + cl->p1 <= p) { // i/o with device 2
					dev = dev2;
					duration = cl->t2;
					device_no = 2;
				}
				
				pthread_mutex_unlock(mutex_sim);
				pthread_mutex_lock(&(dev->mutex));
				pcb->state = PCB_WAITING;
				//while (dev->cur != NULL && dev->cur->p_id != pcb->p_id) {
				while (dev->cur != NULL) {
					pthread_cond_wait(&(dev->cv), &(dev->mutex)); 
				}
				dev->cur = pcb;
				dev->count++;
				
				*should_schedule = 1;
				pthread_cond_signal(cv_sch); // wake up scheduler (case 2)
				
				if (cl->outmode >= OUTMODE_VERBOSE) {
					struct timeval now;
					gettimeofday(&now, NULL);
					long int dif =  (now.tv_sec - start_time.tv_sec) * (1000) + (now.tv_usec - start_time.tv_usec) / (1000); 
					printf("%ld\t", dif);
					printf("%d\t", pcb->p_id);
					printf("Process %d using device %d\n", pcb->p_id, device_no);
				}
				
				//pthread_mutex_unlock(mutex_sim);
				usleep(duration * 1000);
				//pthread_mutex_lock(mutex_sim);
				
				dev->cur = NULL;
				dev->count--;
				
				pthread_cond_broadcast(&(dev->cv));
				
				/*
				while (!(*should_schedule)) {
					pthread_cond_wait(cv_proc);
				}
				*/
				pthread_mutex_unlock(&(dev->mutex));
				pthread_mutex_lock(mutex_sim);
				
				
			}
			
		}
		
		if (p > cl->p0) {
			pcb->state = PCB_READY;
			enqueue(cpu->rq, pcb);
			(*should_schedule) = 1;
			pthread_cond_signal(cv_sch); // wake up scheduler (case 4)
		}
			
		//}
		
		pcb->state = PCB_READY;
		pthread_mutex_unlock(mutex_sim);
	} while (p > cl->p0);
	
	pthread_mutex_lock(mutex_sim);
	(*should_schedule) = 1;
	pthread_cond_signal(cv_sch); // wake up scheduler (case 1)
	if (cl->outmode >= OUTMODE_VERBOSE) {
		printf("Process %d terminated (total returns: %d)\n", pcb->p_id, ++totalreturns);
	}
	return_pid(((process_arg *) args)->pid_list, pcb->p_id);
	//sem_post(((process_arg *) args)->emptyprocs);
	pthread_mutex_unlock(mutex_sim);
	
	pthread_exit(NULL);
}

#endif

