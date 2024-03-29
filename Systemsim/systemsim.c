#include "cl_args.h"
#include "process.h"
#include "system.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <pthread.h>
 
cl_args *cl; // command line args

struct timeval start_time; 
void** returns;

// hardware structs
cpu *sim_cpu;

io_device *dev1;
io_device *dev2;

pthread_t *tids;
pcb **pcbs;

// mutex for process generator and cpu scheduler threads
pthread_mutex_t mutex_sim; // controls access to many simulation resources/structs (essentially most global variables in this file)

//sem_t *sem_fullprocs; // current open processes in the system (between 0 and max_p)
//sem_t *sem_emptyprocs; // current available slots for processes (= max_p - fullprocs)

pthread_cond_t cv_sch; // scheduler cv

pthread_t pgen; // process generator
pthread_t sched; // scheduler

int pgen_done = 0; // set to 1 when process generator finishes
int cur = 0; // all processes that haven't finished yet
int should_schedule = 0;

pid_list *pids; // tracks available pid's
process_arg pargs;

//pcb *arr; //array to print out the results 


// process generator thread (only one will be created)
static void *process_generator(void *args) {
	if (cl->outmode >= OUTMODE_VERBOSE) {
		printf("process_generator() start\n");
	}
	// numeric variables
	int total = 0;

	// random number generator init
	time_t t;
	srand((unsigned) time(&t)); 
	
	struct timeval now; // for time calculations
	// argumental variables for process creations
	
	pcb *newpcb;
	// initial generation loop
	
	pthread_mutex_lock(&mutex_sim);
	for (total = 0; total < INITIAL_PROCESSES; total++) {
		// 1. wait until less than max_p processes exist in the system
		//sem_wait(sem_emptyprocs);
		
		// 2. create PCB for new process
		gettimeofday(&now, NULL);
		long int dif =  (now.tv_sec - start_time.tv_sec) * (1000) + (now.tv_usec - start_time.tv_usec) / (1000); 
		newpcb = pcb_create(pick_pid(&pids), PCB_READY, dif, cl->burst_dist, cl->burst_len, cl->min_burst, cl->max_burst); // TODO set other attributes (tid etc)
		pcbs[total] = newpcb;
		//newpcb = pcb_create(total + 1, PCB_READY, dif); // TODO set other attributes (tid etc)
		
		// 3. add new process to ready queue
		//enqueue(sim_cpu->rq, newpcb);

		// 4. create process thread
		process_arg *pargs = malloc(sizeof(process_arg)); // done in the start of this method
		pargs->cl = cl;
		pargs->cpu = sim_cpu;
		pargs->dev1 = dev1;
		pargs->dev2 = dev2;
		pargs->cv_sch = &cv_sch;
		pargs->mutex_sim = &mutex_sim;
		pargs->pid_list = &pids;
		pargs->start_time = start_time;
		//pargs->emptyprocs = sem_emptyprocs;
		pargs->should_schedule = &should_schedule;
		pargs->cur = &cur;
		pargs->total = &total;
		pargs->pcb = newpcb;
		
		pthread_create(&(tids[total]), NULL, process_th, (void *) pargs);
		cur++;
		if (cl->outmode >= OUTMODE_VERBOSE) {
			printf("Process generated with pid %d (total: %d)\n", newpcb->p_id, total + 1);
			printf("===========================================\n");
		}
		
		// 5. alert scheduler (case 5)
		
	}
	//should_schedule = 1;
	//pthread_cond_signal(&cv_sch);
	pthread_mutex_unlock(&mutex_sim);

	//sem_getvalue(sem_fullprocs, &cur);
	//pthread_mutex_unlock(&mutex_sim);
	
	
	// process generation loop
	printf("generate others\n");
	while (total < cl->all_p) { // simulation ends when (ALL_P) processes are generated and all of them leave the system
		// sleep for 5 ms
		usleep(5000);
		// generate process
		pthread_mutex_lock(&mutex_sim);
		if (cur < cl->max_p && total < cl->all_p && (double) rand() / (double) RAND_MAX < cl->pg) {
			// 1. wait until less than max_p processes exist in the system
			//sem_wait(sem_emptyprocs);
			
			
			// 2. create PCB for new process
			gettimeofday(&now, NULL);
			long int dif =  (now.tv_sec - start_time.tv_sec) * (1000) + (now.tv_usec - start_time.tv_usec) / (1000); 
			newpcb = pcb_create(pick_pid(&pids), PCB_READY, dif, cl->burst_dist, cl->burst_len, cl->min_burst, cl->max_burst); // TODO set other attributes (tid etc)
			pcbs[total] = newpcb;
			//newpcb = pcb_create(total + 1, PCB_READY, dif); // TODO set other attributes (tid etc)
			total++;
			// 3. add new process to ready queue
			//enqueue(sim_cpu->rq, newpcb);

			// 4. create process thread
			// process_arg pargs; // done in the start of this method
			//pargs.pcb = newpcb;
			process_arg *pargs = malloc(sizeof(process_arg));
			pargs->cl = cl;
			pargs->cpu = sim_cpu;
			pargs->dev1 = dev1;
			pargs->dev2 = dev2;
			pargs->cv_sch = &cv_sch;
			pargs->mutex_sim = &mutex_sim;
			pargs->pid_list = &pids;
			pargs->start_time = start_time;
			//pargs->emptyprocs = sem_emptyprocs;
			pargs->should_schedule = &should_schedule;
			pargs->cur = &cur;
			pargs->total = &total;
			pargs->pcb = newpcb;
			pthread_create(&(tids[total - 1]), NULL, process_th, (void *) pargs);
			cur++;
			
			if (cl->outmode >= OUTMODE_VERBOSE) {
				printf("%ld: Process generated with pid %d (total: %d)\n", dif, newpcb->p_id, total);
			}
			
			// 5. alert scheduler (case 5) <- done inside process thread
			//should_schedule = 1;
			//pthread_cond_signal(&cv_sch);

			
			//sem_post(sem_fullprocs);
		}
		pthread_mutex_unlock(&mutex_sim);
		//sem_getvalue(sem_fullprocs, &cur);
	}
	//pthread_mutex_unlock(&mutex_sim);
	
	pthread_mutex_lock(&mutex_sim);
	pgen_done = 1;
	pthread_mutex_unlock(&mutex_sim);
	
	if (cl->outmode >= OUTMODE_VERBOSE) {
		printf("process_generator() end\n");
	}
	pthread_exit(NULL);
}

// cpu scheduler thread (only one will be created)
static void *cpu_scheduler(void *args) {
	if (cl->outmode >= OUTMODE_VERBOSE) {
		printf("cpu_scheduler() start\n");
	}

	// should keep running until ready queue is empty and no more processes will arrive (process_generator is terminated)
	while (!pgen_done || cur > 0) { 
		pthread_mutex_lock(&mutex_sim);
	
		// while not running, sleep on cv
		// scheduler is woken up when:
			// 1. the running thread terminates
			// 2. the running thread starts i/o
			// 3. time quantum for running thread expires (RR)
			// 4. a process returns from i/o and is added to the ready queue
			// 5. a new process is created and is added to the ready queue
		// signals for above cases are sent from elsewhere
		
		while (!should_schedule) { // TODO other conds?
		//while (sim_cpu->cur != NULL || !should_schedule) {
			pthread_cond_wait(&cv_sch, &mutex_sim);
		}
		
		// when waken up
		// TODO check if scheduling needed 
		
		// if so, select process from ready queue
		if (sim_cpu->rq->length > 0) {	
			//sim_cpu->cur = dequeue(sim_cpu->rq);
			sim_cpu->cur = peek(sim_cpu->rq);
			sim_cpu->cur->state = PCB_RUNNING;
			if (cl->outmode >= OUTMODE_VERBOSE) {
				printf("Scheduler runs %d (next burst: %d) (current: %d)\n", sim_cpu->cur->p_id, sim_cpu->cur->next_burst_len, sim_cpu->rq->length);
			}
			// wake up all processes (including selected) with broadcast
			//should_schedule = 0;
			
			pthread_cond_broadcast(&(sim_cpu->rq->cv));
		}
		should_schedule = 0;
		// TODO all processes check whether they're selected. if not, sleep again (this part can be done in the process instead of here)
		
		
		// selected thread enters the cpu (TODO maybe already done above?)
		pthread_mutex_unlock(&mutex_sim);
	}
	
	if (cl->outmode >= OUTMODE_VERBOSE) {
		printf("cpu_scheduler() end\n");
	}
	pthread_exit(NULL);
}

void sim_init() {
	if (cl->outmode >= OUTMODE_VERBOSE) {
		printf("sim_init() start\n");
	}
	
	tids = malloc(cl->all_p * sizeof(pthread_t));
	pcbs = malloc(cl->all_p * sizeof(pcb *));
	//arr = malloc(cl->all_p * sizeof(pcb));
	int i = 0;
	returns = malloc(cl->all_p*sizeof(void*));
	/*
	for (; i<cl->all_p; i++){
		arr[i] = malloc(sizeof(pcb));
	}
	*/

	// semaphores
	//sem_unlink(SEMNAME_MUTEX_SIM);
	//sem_unlink(SEMNAME_FULLPROCS);
	//sem_unlink(SEMNAME_EMPTYPROCS);
	
	//sem_mutex_sim = sem_open(SEMNAME_MUTEX_SIM, O_RDWR | O_CREAT, 0660, 1);
	//sem_fullprocs = sem_open(SEMNAME_FULLPROCS, O_RDWR | O_CREAT, 0660, 0);
	//sem_emptyprocs = sem_open(SEMNAME_EMPTYPROCS, O_RDWR | O_CREAT, 0660, cl->max_p);
	
	// conditional variables
	pthread_cond_init(&cv_sch, NULL);
	
	// mutexes
	pthread_mutex_init(&mutex_sim, NULL);
	
	// hardware structs
	cpu_init(&sim_cpu, cl->alg);
	io_device_init(&dev1);
	io_device_init(&dev2);
	
	// pid_list
	pid_list_init(&pids, 100);
	
	if (cl->outmode >= OUTMODE_VERBOSE) {
		printf("sim_init() end\n");
	}
}

void sim_begin() {
	if (cl->outmode >= OUTMODE_VERBOSE) {
		printf("sim_begin() start\n");
	}
	gettimeofday(&start_time, NULL);
	// simulator threads
	pthread_create(&pgen, NULL, process_generator, NULL);
	pthread_create(&sched, NULL, cpu_scheduler, NULL);
	
	
	
	if (cl->outmode >= OUTMODE_VERBOSE) {
		printf("sim_begin() end\n");
	}
}

void sim_end() {
	int i;
	pcb* new;
	for (i = 0; i < cl->all_p; i++) {
		pthread_join(tids[i], &(returns[i]));
	}
	pthread_join(pgen, NULL); // TODO return may not be null (so that info can be carried etc.)
	pthread_join(sched, NULL);
	printf("pid\tarv\tdept\tcpu\twaitr\tturna\tn-bursts\tn-d1\tn-d2\n");
	for (i = 0; i< cl->all_p; i++){
		new = (pcb*)returns[i];
		printf("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t\n", new->p_id, new->start_time, new->finish_time, new->total_time, (new->finish_time-new->start_time-new->total_time), (new->finish_time-new->start_time),new->bursts_completed, new->n1, new->n2);
		free(returns[i]);
	}
	for (i = 0; i < cl->all_p; i++) {
		free(pcbs[i]);
	}
	free(pcbs);
	
	free(returns);
	//printf("pid\tarv\tdept\tcpu\twaitr\tturna\tn-bursts\tn-d1\tn-d2");
	
	// frees
	pid_list_delete(&pids);
	cpu_delete(sim_cpu);
	io_device_delete(dev1);
	io_device_delete(dev2);
	free(cl);
	free(tids);
	pthread_mutex_destroy(&mutex_sim);
	pthread_cond_destroy(&cv_sch);
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


