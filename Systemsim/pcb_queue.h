#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>

// process state enum
#define PCB_RUNNING 1
#define PCB_WAITING 2
#define PCB_READY 3

typedef struct {
	int p_id;
	int state;
	pthread_t t_id;
	int next_burst_len;
	int remaining_burst_len;
	int bursts_completed;
	int start_time;
	int finish_time;
	int total_time;
} pcb;

pcb *pcb_create(int p_id, int state, int start_time) {
	pcb *newpcb = malloc(sizeof(pcb));
	newpcb->p_id = p_id;
	newpcb->state = state;
	newpcb->remaining_burst_len = 0;
	newpcb->bursts_completed = 0;
	newpcb->start_time = start_time;
	newpcb->finish_time = -1;
	newpcb->total_time = 0;
}

typedef struct {
	pcb *item;
	pcb_node *prev;
	pcb_node *next;
} pcb_node;

typedef struct {
	int count = 0;
	pcb_node *head = NULL;
	pcb_node *tail = NULL;
} pcb_queue;

void enqueue(pcb_queue *queue, pcb *item) {
	pcb_node *tmp = malloc(sizeof(pcb_node));
	tmp->item = item;
	
	tmp->prev = queue->tail;
	tmp->next = NULL;
	
	if (queue->count == 0) {
		queue->head = tmp;
	}
	else {
		queue->tail->next = tmp;
	}
	
	queue->tail = tmp;
	queue->count++;
}

// dequeue mode
#define MODE_FIFO 1
#define MODE_PRIO 2

pcb *dequeue(pcb_queue *queue, int mode) {
	if (queue->count == 0) {
		return NULL;
	}
	else if (mode == MODE_FIFO) {
		pcb *rv = queue->tail->item;
		pcb_node *tmp = queue->tail; 
		queue->tail = queue->tail->prev;
		queue->tail->next = NULL;
		free(tmp);
		queue->count--;
		return rv;
	}
	else if (mode == MODE_PRIO) {
		pcb *rv = queue->head->item;
		pcb_node *cur, *tmp;
		
		for (cur = queue->head; cur != NULL; cur = tmp->next) {
			if (cur->item->next_burst_len < rv->next_burst_len) {
				tmp = cur;
				rv = cur->item;
			}
		}
		
		free(tmp);
		queue->count--;
	}
	else {
		return NULL;
	}
}

pcb *get_pcb(pcb_queue *queue, pthread_t tid){
	pcb* tmp = queue->head;
	for (; tmp != NULL; tmp = tmp->next){
		if (tmp->item->t_id == tid)
			return tmp->item;
	}

	return NULL;
}

// ready queue
typedef struct {
	pthread_cond_t cv;
	int mode;
	pcb_queue queue;
} ready_queue;

void ready_queue_init(ready_queue **rq, int alg) {
	*rq = malloc(sizeof(ready_queue));
	pthread_cond_init(&(*rq)->cv), NULL);
	(*rq)->mode = alg == ALG_SJF ? MODE_PRIO : MODE_FIFO; // PRIO or FIFO
}

int enqueue(ready_queue *rq, pcb *pcb) {
	if (pcb->state != PCB_READY) {
		return -1; // "ready" queue should only have ready processes
	}
	enqueue(&(rq->queue), pcb);
	return 0;
}

pcb *dequeue(ready_queue *rq) {
	return dequeue(&(rq->queue), rq->mode);
}

// simple linked list of pid's and their states
typedef struct {
	int pid;
	int used;
	pid_list *next;
} pid_list;

int pid_list_init(pid_list **list, int max_pid) {
	int i;
	*list = NULL;
	pid_list *tmp = *list;
	for (i = max_pid; i > 0; i--) {
		tmp = malloc(sizeof(pid_list));
		tmp->pid = i;
		tmp->used = 0;
		tmp->next = *list;
		*list = tmp;
	}
}

int pid_list_delete(pid_list **list) {
	pid_list *tmp;
	while (*list != NULL) {
		tmp = *list;
		*list = *list->next;
		free(tmp);
	}
	*list = NULL;
}

int pick_pid(pid_list **list) {
	pid_list *cur = *list;
	while (cur != NULL) {
		if (cur->used == 0) {
			cur->used = 1;
			return cur->pid;
		}
		cur = cur->next;
	}
	
	return -1;
}

void return_pid(pcb_list **list, int pid) {
	pid_list *cur = *list;
	while (cur != NULL) {
		if (cur->pid == pid) {
			cur->used = 0;
			return;
		}
		cur = cur->next;
	}
}


