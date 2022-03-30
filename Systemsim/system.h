#ifndef SYSTEM_H
#define SYSTEM_H

#define INITIAL_PROCESSES 10

#define SEMNAME_MUTEX_SIM "/name_sem_mutex_sim"
#define SEMNAME_FULLPROCS "/name_sem_fullprocs"
#define SEMNAME_EMPTYPROCS "/name_sem_emptyprocs"

typedef struct cpu {
	ready_queue *rq;
	pcb *cur;
} cpu;

void cpu_init(cpu **cpu, int alg) {
	*cpu = malloc(sizeof(cpu));
	ready_queue_init(&((*cpu)->rq), alg);
	(*cpu)->cur = NULL;
}

typedef struct io_device {
	int count;
	pcb *cur;
	pthread_cond_t *cv;
	pthread_mutex_t *mutex;
} io_device;

void io_device_init(io_device **dev) {
	*dev = malloc(sizeof(io_device));
	(*dev)->count = 0;
	pthread_cond_init((*dev)->cv, NULL);
	pthread_mutex_init((*dev)->mutex, NULL);
	(*dev)->cur = NULL;
}

#endif
