#include "cl_args.h"
#include "process.h"
#include "system.h"

cl_args cl;

cpu cpu;

sem_t *sem_mutex_sim; // mutex for process generator thread
sem_t *sem_fullprocs; // current open processes in the system (between 0 and max_p)
sem_t *sem_emptyprocs; // current available slots for processes (= max_p - fullprocs)

pthread_cond_t cv_sch; // scheduler cv

pthread_thread_t pgen; // process generator
pthread_thread_t sched; // scheduler

// process generator thread (only one will be created)
void *process_generator(void *args) {
	// create initial process threads
	int total = 0;
	for (; total < INITIAL_PROCESSES; ++total) {
		// generate process
	}
	int cur;
	sem_getvalue(sem_fullprocs, &cur);
	
	// set up rng
	time_t t;
	srand((unsigned) time(&t));
	
	// process generation loop
	int n;
	pthread_t tid;
	while (total < cl.all_p || cur != 0) { // simulation ends when (ALL_P) processes are generated and all of them leave the system
		// sleep for 5 ms
		usleep(5);
		
		// generate process
		if (total < cl.all_p && (double) rand() / (double) RAND_MAX < cl.pg) {
			// 1. wait until less than max_p processes exist in the system
			wait(sem_emptyprocs);
			wait(sem_mutex_sim);
			
			// 2. create process thread
			n = pthread_create(&tid, NULL, process_th, (void *) process_args);
			if (n != 0) {
				printf("[ERROR] process_generator failed to create thread\n");
				exit(1);
			}
			
			// 3. create PCB for new process
			pcb *newpcb = pcb_create(total, PCB_READY, tid, 1, 0);
			
			// 4. add new process to ready queue
			enqueue(&rq, newpcb);
			total++;
			
			signal(sem_mutex_sim);
			signal(sem_fullprocs);
		}
		
		sem_getvalue(sem_fullprocs, &cur);
	}
	
	pthread_exit(NULL);
}

// cpu scheduler thread (only one will be created)
void *cpu_scheduler(void *args) {
	// should keep running until ready queue is empty and no more processes will arrive (process_generator is terminated)
	while (pthread_tryjoin_np(pgen, NULL) != 0 && cpu->rq->count > 0) {
		// while not running, sleep on cv
		// scheduler woken up when:
			// TODO the running thread terminates
			// TODO the running thread starts i/o
			// TODO time quantum for running thread expires (RR)
			// TODO a process returns from i/o and is added to the ready queue
			// TODO a new process is created and is added to the ready queue
		
		while (cpu->cur != NULL) { // TODO other conds?
			pthread_cond_wait(&cv_sch);
		}
		
		// when waken up
		
		// TODO check if scheduling needed
		
		// if so, select process from ready queue
		cpu->cur = dequeue(&(cpu->rq));
		
		// wake up all processes (including selected) with broadcast
		pthread_cond_broadcast(&cv_sch);
			
		// TODO all processes check whether they're selected. if not, sleep again (this part may be done in process thread instead of here)
		
		// selected thread enters the cpu (TODO maybe already done above?)
	}
	
	pthread_exit(NULL);
}

void sim_init() {
	// semaphores
	sem_unlink(SEMNAME_MUTEX_SIM);
	sem_unlink(SEMNAME_FULLPROCS);
	sem_unlink(SEMNAME_EMPTYPROCS);
	
	sem_mutex_sim = sem_open(SEMNAME_MUTEX_SIM, O_RDWR | O_CREAT, 0660, 1);
	sem_fullprocs = sem_open(SEMNAME_FULLPROCS, O_RDWR | O_CREAT, 0660, 0);
	sem_emptyprocs = sem_open(SEMNAME_EMPTYPROCS, O_RDWR | O_CREAT, 0660, cl.max_p);
	
	// conditional variables
	pthread_cond_init(&cv_sch, NULL);
	
	// cpu struct
	cpu_init(&cpu);
	
	// simulator threads
}

int main(int argc, char **argv) {
	// init
	process_args(&cl, argc, argv);
	sim_init();
	
	// simulate stuff
	
	return 0;
}


