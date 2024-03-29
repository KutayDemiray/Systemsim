#ifndef SYSTEM_H
#define SYSTEM_H

#include "pcb_queue.h"

#define INITIAL_PROCESSES 10

#define SEMNAME_MUTEX_SIM "/name_sem_mutex_sim"
#define SEMNAME_FULLPROCS "/name_sem_fullprocs"
#define SEMNAME_EMPTYPROCS "/name_sem_emptyprocs"

typedef struct cpu {
	ready_queue *rq;
	pcb *cur;
} cpu;

void cpu_init(cpu **sim_cpu, int alg) {
	*sim_cpu = malloc(sizeof(cpu));
	ready_queue_init(&((*sim_cpu)->rq), alg); 
	(*sim_cpu)->cur = NULL;
}

void cpu_delete(cpu *sim_cpu) {
	ready_queue_delete(sim_cpu->rq);
	if (sim_cpu->cur != NULL) {
		free(sim_cpu->cur);
	}
	free(sim_cpu);
}

typedef struct io_device {
	int count;
	pcb *cur;
	pthread_cond_t cv;
	pthread_mutex_t mutex;
} io_device;

void io_device_init(io_device **dev) {
	*dev = malloc(sizeof(io_device));
	(*dev)->count = 0;
	pthread_cond_init(&((*dev)->cv), NULL);
	pthread_mutex_init(&((*dev)->mutex), NULL);
	(*dev)->cur = NULL;
}

void io_device_delete(io_device *dev) {
	if (dev->cur != NULL) {
		free(dev->cur);
	}
	pthread_cond_destroy(&(dev->cv));
	pthread_mutex_destroy(&(dev->mutex));
	free(dev);
}

#endif
