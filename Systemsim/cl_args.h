// algorithm types
#define ALG_FCFS 1
#define ALG_SJF 2
#define ALG_RR 3

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
	int all_p; // total number of process threads that will be created before simulation ends
	int outmode; // output mode
} cl_args;

void read_args(cl_args *cl, int argc, char **argv) {
	if (strcmp(atoi(argv[1]), "FCFS") == 0) {
		cl->alg = ALG_FCFS;
	}
	else if (strcmp(atoi(argv[1]), "SJF") == 0) {
		cl->alg = ALG_SJF;
	}
	else if (strcmp(atoi(argv[1]), "RR") == 0) {
		cl->alg = ALG_RR;
	}
	else {
		printf("Invalid algorithm arg\n");
		exit(1);
	}
	
	if (cl->alg == ALG_RR) {
		cl->q = INF;
	}
	else {
		cl->q = atoi(argv[2]);
	}
	
	cl->t1 = atoi(argv[3]);
	cl->t2 = atoi(argv[4]);
	
	if (strcmp(atoi(argv[5], "fixed") == 0) {
		cl->burst_dist = FIXED;
	}
	else if (strcmp(argv[5], "uniform") == 0) {
		cl->burst_dist = UNIFORM;
	}
	else if (strcmp(argv[5], "exponential") == 0) {
		cl->burst_dist = EXPONENTIAL;
	}
	else {
		printf("Invalid burst distribution arg\n");
		exit(1);
	}
	
	cl->burst_len = atoi(argv[6]);
	cl->min_burst = atoi(argv[7]);
	cl->max_burst = atoi(argv[8]);
	
	cl->p0 = atof(argv[9]);
	cl->p1 = atof(argv[10]);
	cl->p2 = atof(argv[11]);
	cl->pg = atof(argv[12]);
	
	cl->max_p = atoi(argv[13]);
	cl->all_p = atoi(argv[14]);
	cl->outmode = atoi(argv[15]);
}
