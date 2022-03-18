#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>

// process state enum
#define RUNNING 1
#define WAITING 2
#define READY 3

typedef struct {
	int p_id;
	int state;
	int thread_id;
	int next_burst_len;
	int total_time; 
} pcb;

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
}

// dequeue mode
#define FIFO 0
#define SJF 1

pcb *dequeue(pcb_queue *queue, int mode) {
	if (queue->count == 0) {
		return NULL;
	}
	else if (mode == FIFO) {
		pcb *rv = queue->tail->item;
		pcb_node *tmp = queue->tail; 
		queue->tail = queue->tail->prev;
		queue->tail->next = NULL;
		free(tmp);
		return rv;
	}
	else if (mode == SJF) {
		pcb *rv = queue->head->item;
		pcb_node *tmp;
		for (tmp = queue->head; tmp != NULL; tmp = tmp->next) {
			if (tmp->item->next_burst_len < rv->next_burst_len) {
				rv = tmp->item;
			}
		}
	}
}

// by default, use FIFO for dequeue
pcb *dequeue(pcb_queue *queue) {
	return dequeue(queue, FIFO);
}

