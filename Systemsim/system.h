#define INITIAL_PROCESSES 10

#define SEMNAME_MUTEX_SIM "/name_sem_mutex_sim"
#define SEMNAME_FULLPROCS "/name_sem_fullprocs"
#define SEMNAME_EMPTYPROCS "/name_sem_emptyprocs"

typedef struct {
	ready_queue rq;
	pcb *cur;
} cpu;

void cpu_init(cpu *cpu, int alg) {
	ready_queue_init(&(cpu->rq), cl_alg);
	(*cpu)->cur = NULL;
}

typedef struct {
	int count;
	pthread_cond_t cv;
} io_device;

void io_device_init(io_device *dev) {
	dev->count = 0;
	pthread_cond_init(&(dev->cv));
}
