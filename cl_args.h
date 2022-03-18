// algorithm types
#define FCFS 1
#define SJF 2
#define RR 3

#define INF -1 // infinity

// device service time bounds
#define D1_MIN_SERVICE_TIME 30
#define D1_MAX_SERVICE_TIME 100
#define D2_MIN_SERVICE_TIME 100
#define D2_MAX_SERVICE_TIME 300

// burst distribution
#define FIXED 1
#define EXPONENTIAL 2
#define UNIFORM 3

// process number bounds
#define MIN_MAX_P 1
#define MAX_MAX_P 50
#define MIN_ALL_P 1
#define MAX_ALL_P 1000

// outmodes
#define OUTMODE_NOTHING 1
#define OUTMODE_BASIC 2
#define OUTMODE_VERBOSE 3

typedef struct {
	int alg; // scheduling algorithm
	int q; // time quantum (infinite for FCFS and SJF)
	int t1, t2; // service times of device 1 and device 2
	int burst_dist; // burst distribution
	int burst_len, min_burst, max_burst; // burst params
	double p0; // probability that a running thread will terminate
	double p1; // probability that a running thread will go for i/o with device 1
	double p2; // probability that a running thread will go for i/o with device 2
	double pg; // probability that a new thread is generated
	int max_p; // maximum number of processes that can exist simultaneously
	int all_p; // number of process threads that will be created before simulation ends
	int outmode;
} cl_args;

cl_args *process_args(int argc, char **argv) {
	// process args
}
