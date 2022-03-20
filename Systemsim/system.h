#define INITIAL_PROCESSES 10

#define SEMNAME_MUTEX_SIM "/name_sem_mutex_sim"
#define SEMNAME_FULLPROCS "/name_sem_fullprocs"
#define SEMNAME_EMPTYPROCS "/name_sem_emptyprocs"

typedef struct {
	ready_queue rq;
} cpu;
