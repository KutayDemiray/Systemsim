#ifndef PCB_QUEUE_H
#define PCB_QUEUE_H

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <math.h>
#include "cl_args.h"

// process state enum
#define PCB_RUNNING 1
#define PCB_WAITING 2
#define PCB_READY 3

// dequeue mode
#define MODE_FIFO 1
#define MODE_PRIO 2 

typedef struct pcb {
	int p_id;
	int state;
	pthread_t t_id;
	int next_burst_len;
	int remaining_burst_len;
	int bursts_completed;
	int start_time;
	int finish_time;
	int total_time;
	int n1;
	int n2;
} pcb;

pcb *pcb_create(int p_id, int state, int start_time, int burst_dist, int burst_len, int min_burst, int max_burst) {
	pcb *newpcb = malloc(sizeof(pcb));
	newpcb->p_id = p_id;
	newpcb->state = state;
	
	// determine next cpu burst length
	double u, next;
	if (burst_dist == UNIFORM) {
		u = ((double) (rand() % 1000)) / 1000.0;
		next = min_burst + (int) ((max_burst - min_burst) * u);
	}
	else if (burst_dist == EXPONENTIAL) { // exponential distribution
		// algorithm below is taken from prof. korpeoglu's forum post
		// exponential distribution with lambda = 1/burst_len
		do {
			u = ((double) (rand() % 1000)) / 1000.0;
			next = (int) ((-1) * log(u) * burst_len);
		} while (next < min_burst || next > max_burst);
	}
	else if (burst_dist == FIXED) { // fixed
		next = burst_len;
	}

	newpcb->next_burst_len = next;
	newpcb->remaining_burst_len = next; 
	
	newpcb->bursts_completed = 0;
	newpcb->start_time = start_time;
	newpcb->finish_time = -1;
	newpcb->total_time = 0;
	newpcb->n1 = 0;
	newpcb->n2 = 0;
	return newpcb;
}

typedef struct pcb_node {
	pcb *item;
	struct pcb_node *prev;
	struct pcb_node *next;
} pcb_node;

typedef struct pcb_queue {
	pcb_node *head;
	pcb_node *tail;
} pcb_queue;

void pcb_queue_init(pcb_queue **queue) {
	(*queue) = malloc(sizeof(pcb_queue));
	(*queue)->head = NULL;
	(*queue)->tail = NULL;
}

void enqueue_node(pcb_queue **queue, pcb *item) {
	pcb_node *tmp = malloc(sizeof(pcb_node));
	tmp->item = item;
	
	//tmp->prev = queue->tail;
	//tmp->next = NULL;
	
	tmp->next = (*queue)->head; 
	tmp->prev = NULL;
	if ((*queue)->head == NULL) {
		(*queue)->tail = tmp;
	}
	else {
		(*queue)->head->prev = tmp;
	}
	(*queue)->head = tmp;
	
	//queue->tail = tmp;
}

pcb *dequeue_node(pcb_queue **queue, int mode) {
	if ((*queue)->head == NULL || (*queue)->tail == NULL) {
		return NULL;
	}
	else if (mode == MODE_FIFO) {
		pcb *rv = (*queue)->tail->item;
		pcb_node *tmp = (*queue)->tail; 
		(*queue)->tail = (*queue)->tail->prev;
		if ((*queue)->tail != NULL) {
			(*queue)->tail->next = NULL;
		}
		else {
			(*queue)->head = NULL;
		}
		free(tmp);
		//printf("dequeue_node() -> %d done\n", rv->p_id);
		return rv;
	}
	else if (mode == MODE_PRIO) {
		pcb *rv = (*queue)->head->item;
		pcb_node *cur;
		pcb_node *tmp = NULL;
		
		for (cur = (*queue)->head; cur != NULL; cur = cur->next) {
			if (cur->item->next_burst_len < rv->next_burst_len) {
				tmp = cur;
				rv = cur->item;
			}
		}
		
		if (tmp != NULL && tmp->prev != NULL) {
			tmp->prev->next = tmp->next;
		}
		if (tmp != NULL && tmp->next != NULL) {
			tmp->next->prev = tmp->prev;
		}
		
		free(tmp);
		//printf("dequeue_node() -> %d\n", rv->p_id);
		return rv;
	}
	else {
		return NULL;
	}
}

pcb *peek_node(pcb_queue *queue, int mode) {
	if (queue->head == NULL || queue->tail == NULL) {
		return NULL;
	}
	else if (mode == MODE_FIFO) {
		pcb *rv = queue->tail->item;
		//printf("dequeue_node() -> %d done\n", rv->p_id);
		return rv;
	}
	else if (mode == MODE_PRIO) {
		
		pcb *rv = queue->head->item;
		//printf("===========================================================%d %d\n", queue->head->item->p_id, rv->next_burst_len);
		pcb_node *cur;
		
		for (cur = queue->head; cur != NULL; cur = cur->next) {
			printf("%d ", cur->item->next_burst_len);
			printf("%d ", rv->next_burst_len);
			if (cur->item->next_burst_len < rv->next_burst_len) {
				rv = cur->item;
			}
		}
		
		//printf("dequeue_node() -> %d\n", rv->p_id);
		return rv;
	}
	else {
		return NULL;
	}
}

void pcb_queue_delete(pcb_queue *q) {
	while (q->tail != NULL) {
		free(dequeue_node(&q, MODE_FIFO));
	}
	free(q);
	
}

pcb *get_pcb(pcb_queue *queue, pthread_t tid){
	pcb_node* tmp = queue->head;
	for (; tmp != NULL; tmp = tmp->next){
		if (tmp->item->t_id == tid)
			return tmp->item;
	}

	return NULL;
}

// ready queue
typedef struct ready_queue {
	pthread_cond_t cv;
	int mode;
	int length;
	pcb_queue *queue;
} ready_queue;

void ready_queue_init(ready_queue **rq, int alg) {
	*rq = malloc(sizeof(ready_queue));
	pcb_queue_init(&((*rq)->queue));
	(*rq)->length = 0;
	pthread_cond_init(&((*rq)->cv), NULL);
	(*rq)->mode = alg == ALG_SJF ? MODE_PRIO : MODE_FIFO; // PRIO or FIFO
}

void ready_queue_delete(ready_queue *rq) {
	pthread_cond_destroy(&(rq->cv));
	pcb_queue_delete(rq->queue);
	free(rq);
}

void enqueue(ready_queue *rq, pcb *pcb) {
	if (pcb->state != PCB_READY) {
		printf("Cannot enqueue a process that's not ready to the ready queue\n"); // "ready" queue should only have ready processes
		exit(1);
	}
	enqueue_node(&(rq->queue), pcb);
	rq->length++;
}

pcb *dequeue(ready_queue *rq) {
	if (rq->length > 0)
		rq->length--;
	return dequeue_node(&(rq->queue), rq->mode);
}

pcb *peek(ready_queue *rq) {
	return peek_node(rq->queue, rq->mode);
}
// simple linked list of pid's and their states
typedef struct pid_list {
	int pid;
	int used;
	struct pid_list *next;
} pid_list;

void pid_list_init(pid_list **list, int max_pid) {
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

void pid_list_delete(pid_list **list) {
	pid_list *tmp;
	while (*list != NULL) {
		tmp = *list;
		*list = (*list)->next;
		free(tmp);
	}
	*list = NULL;
}

int pick_pid(pid_list **list) {
	pid_list *cur = *list;
	while (cur != NULL) {
		if (cur->used == 0) {
			cur->used = 1;
			//printf("picked %d\n", cur->pid);
			return cur->pid;
		}
		cur = cur->next;
	}
	
	return -1;
}

void return_pid(pid_list **list, int pid) {
	pid_list *cur = *list;
	while (cur != NULL) {
		if (cur->pid == pid) {
			cur->used = 0;
			return;
		}
		cur = cur->next;
	}
}

#endif
