#include "cl_args.h"
#include "process.h"
#include "system.h"
#include <semaphore.h>
#include <pthread.h>

cl_args cl; // command line args

struct timeval start_time;

// hardware structs
cpu *sim_cpu;

io_device *dev1;
io_device *dev2;

// mutex for process generator and cpu scheduler threads
pthread_mutex_t *mutex_sim; // controls access to many simulation resources/structs (essentially most global variables in this file)

sem_t *sem_fullprocs; // current open processes in the system (between 0 and max_p)
sem_t *sem_emptyprocs; // current available slots for processes (= max_p - fullprocs)

pthread_cond_t *cv_sch; // scheduler cv

pthread_t pgen; // process generator
pthread_t sched; // scheduler

pid_list *pids; // tracks available pid's

// process generator thread (only one will be created)
void *process_generator(void *args) {
	// numeric variables
	int total;
	int n;

	// random number generator init
	time_t t;
	srand((unsigned) time(&t));
	
	struct timeval now; // for time calculations
	// argumental variables for process creations
	pthread_t tid;
	process_arg pargs;
	pargs.cl = &cl;
	pargs.cpu = sim_cpu;
	pargs.dev1 = dev1;
	pargs.dev2 = dev2;
	pargs.cv_sch = cv_sch;
	pargs.mutex_sim = mutex_sim;
	pargs.pid_list = &pids;
	
	// initial generation loop
	pthread_mutex_lock(mutex_sim);
	for (total = 0; total < INITIAL_PROCESSES; ++total) {
		// generate initial process
		// 1. wait until less than max_p processes exist in the system

		// 3. create PCB for new process
		gettimeofday(&now, NULL);
		int dif = 1000 * (now.tv_sec - start_time.tv_sec) + now.tv_usec - start_time.tv_usec; 
		pcb *newpcb = pcb_create(pick_pid(&pids), PCB_READY, dif);
		
		// 4. add new process to ready queue
		enqueue(sim_cpu->rq, newpcb);
		total++;

		// 2. initialize process thread
		pargs.pcb = newpcb;
		
		if (cl.outmode >= OUTMODE_VERBOSE){
			printf("Process with id %lu is created at time", tid);
		}
		
		pthread_create(&tid, NULL, process_th, (void *) &pargs);
		
		// 5. alert scheduler (case 5)
		pthread_cond_signal(cv_sch);
	}
	pthread_mutex_unlock(mutex_sim);

	int cur;
	sem_getvalue(sem_fullprocs, &cur);
	pthread_mutex_unlock(mutex_sim);
	
	// process generation loop
	while (total < cl.all_p || cur != 0) { // simulation ends when (ALL_P) processes are generated and all of them leave the system
		if (cl.outmode >= OUTMODE_BASIC){
			pcb* tmp = get_pcb(gettid());
			printf("%u \t %", (unsigned int) (gettimeofday(&now, NULL) - start_time));
			printf(tmp->pid);
			printf("\t");
			case(tmp->state)
				1: printf("RUNNING");
				2: printf("WAITING");
				3: printf("READY");
			printf("\n");
		}
		
		// sleep for 5 ms
		usleep(5000);
		
		// generate process
		if (total < cl.all_p && (double) rand() / (double) RAND_MAX < cl.pg) {
			// 1. wait until less than max_p processes exist in the system
			sem_wait(sem_emptyprocs);
			pthread_mutex_lock(mutex_sim);
			
			// 2. create PCB for new process
			newpcb = pcb_create(pick_pid(&pid_list), PCB_READY, tid, cl->min_burst + cl->burst_len, (int) (gettimeofday(&t, NULL) - start_time), 0);
			
			// 3. add new process to ready queue
			enqueue(&rq, newpcb);
			total++;

			// 4. create process thread
			// process_arg pargs; // done in the start of this method
			pargs.pcb = newpcb;
			
			n = pthread_create(&tid, NULL, process_th, (void *) pargs);
			if (n != 0) {
				printf("[ERROR] process_generator failed to create thread\n");
				exit(1);
			}
			
			// 3. create PCB for new process
			pcb *newpcb = pcb_create(pick_pid(&pid_list), PCB_READY, (int) (gettimeofday(&t, NULL) - start_time));
			
			// 4. add new process to ready queue
			enqueue(&rq, newpcb);
			total++;
			
			// 5. alert scheduler (case 5)
			pthread_cond_signal(cv_sch);

			pthread_mutex_unlock(mutex_sim);
			sem_post(sem_fullprocs);
		}
		
		sem_getvalue(sem_fullprocs, &cur);
	}
	
	pthread_exit(NULL);
}

// cpu scheduler thread (only one will be created)
void *cpu_scheduler(void *args) {
	// should keep running until ready queue is empty and no more processes will arrive (process_generator is terminated)
	while (pthread_tryjoin_np(pgen, NULL) != 0 && cpu->rq->count > 0) {
		pthread_mutex_lock(mutex_sim);
	
		// while not running, sleep on cv
		// scheduler is woken up when:
			// 1. the running thread terminates
			// 2. the running thread starts i/o
			// 3. time quantum for running thread expires (RR)
			// 4. a process returns from i/o and is added to the ready queue
			// 5. a new process is created and is added to the ready queue
		// signals for above cases are sent from elsewhere
		
		while (cpu->cur != NULL) { // TODO other conds?
			pthread_cond_wait(cv_sch, mutex_sim);
		}
		
		// when waken up
		
		// TODO check if scheduling needed
		
		// if so, select process from ready queue
		cpu->cur = dequeue(&(cpu->rq));
		
		// wake up all processes (including selected) with broadcast
		pthread_cond_broadcast(cv_sch);
			
		// TODO all processes check whether they're selected. if not, sleep again (this part can be done in the process instead of here)
		
		// selected thread enters the cpu (TODO maybe already done above?)
		
		pthread_mutex_unlock(mutex_sim);
	}
	
	pthread_exit(NULL);
}

void sim_init() {
	// semaphores
	//sem_unlink(SEMNAME_MUTEX_SIM);
	sem_unlink(SEMNAME_FULLPROCS);
	sem_unlink(SEMNAME_EMPTYPROCS);
	
	//sem_mutex_sim = sem_open(SEMNAME_MUTEX_SIM, O_RDWR | O_CREAT, 0660, 1);
	sem_fullprocs = sem_open(SEMNAME_FULLPROCS, O_RDWR | O_CREAT, 0660, 0);
	sem_emptyprocs = sem_open(SEMNAME_EMPTYPROCS, O_RDWR | O_CREAT, 0660, cl.max_p);
	
	// conditional variables
	pthread_cond_init(cv_sch, NULL);
	
	// mutexes
	pthread_mutex_init(mutex_sim, NULL);
	
	// hardware structs
	cpu_init(&cpu);
	io_device_init(&dev1);
	io_device_init(&dev2);
	
	// pid_list
	pid_list_init(&pid_list, cl->max_p);
}

void sim_begin() {
	// simulator threads
	pthread_create(&pgen, NULL, process_generator, NULL);
	pthread_create(&sched, NULL, cpu_scheduler, NULL);
	
	gettimeofday(&start_time, NULL);
}

void sim_end() {
	pthread_join(pgen, NULL); // TODO return may not be null (so that info can be carried etc.)
	pthread_join(sched, NULL);
	
	printf("pid\tarv\tdept\tcpu\twaitr\tturna\tn-bursts\tn-d1\tn-d2");
	
	
	// frees
	pid_list_delete(&pid_list);
}

int main(int argc, char **argv) {
	printf("main\n");
	// init
	read_args(&cl, argc, argv);
	sim_init();
	
	sim_begin(); // start simulation threads
	// simulate stuff
	sim_end(); // print information to console too
	printf("main end\n");
	return 0;
}


