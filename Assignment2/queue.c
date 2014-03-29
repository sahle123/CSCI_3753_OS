/*
 * File: queue.c
 * Author: Chris Wailes <chris.wailes@gmail.com>
 * Author: Wei-Te Chen <weite.chen@colorado.edu>
 * Author: Andy Sayler <andy.sayler@gmail.com>
 * Project: CSCI 3753 Programming Assignment 2
 * Create Date: 2010/02/12
 * Modify Date: 2011/02/04
 * Modify Date: 2012/02/01
 * Modify Date: 2014/28/02
 * Description:
 * 	This file contains an implementation of a simple FIFO queue.
 *  
 */

#include <stdlib.h>

#include "queue.h"

int queue_init(queue* q, int size){
	
	int i;

	/* user specified size or default */
	if(size>0) {
	   q->maxSize = size;
	}
	else {
	   q->maxSize = QUEUEMAXSIZE;
	}

	/* malloc array */
	q->array = malloc(sizeof(queue_node) * (q->maxSize));
	if(!(q->array)){	
	   perror("Error on queue Malloc");
	   return QUEUE_FAILURE;
	}

	/* Set to NULL */
	for(i=0; i < q->maxSize; ++i){
	   q->array[i].payload = NULL;
	}

	/* setup circular buffer values */
	q->front = 0;
	q->rear = 0;

	/* Allocate onto the stack */
	q->contents = malloc(sizeof(sem_t));    // how many elements are currently on the queue. 
											// contents counts up from 0 --> maxSize.
	q->freeSpace = malloc(sizeof(sem_t));   // how many more elements can be pushed onto the queue.
											// freeSpace counts down from maxSize --> 0. 
	q->mutex = malloc(sizeof(pthread_mutex_t)); // used so that only one thread can access a element at a time. 
												// call on wait or signal. 
	q->finished = 0;

	/* Initialize semaphores */
	sem_init(q->contents, 0, 0);				
	sem_init(q->freeSpace, 0, q->maxSize);

	/* Initialize the mutex. */
	pthread_mutex_init(q->mutex, NULL);

	return q->maxSize;
}

int queue_is_empty(queue* q){
	if((q->front == q->rear) && (q->array[q->front].payload == NULL)){
		return 1;
	}
	else{
		return 0;
	}
}

int queue_is_full(queue* q){
	if((q->front == q->rear) && (q->array[q->front].payload != NULL)){
	return 1;
	}
	else{
	return 0;
	}
}

void* queue_pop(queue* q){
	void* ret_payload;

	/* if you are done poping elements and the queue is empty, do nothing. */
	/* you are done popping elements when you call queue_close. */
	if(q->finished && queue_is_empty(q))
		return NULL;
	
	// waiting until it frees up
	// wait for contents > 0. 
	while(sem_wait(q->contents)); //--content

	/* check to make sure you did not finish while waiting. */
	if(q->finished && queue_is_empty(q))
		return NULL;

	//  mutex locks it
	pthread_mutex_lock(q->mutex);

	sem_post(q->freeSpace);	// ++freeSpace. Unblocks any blocked threads in queue_push.
	
	// add a new element to the circular array and move the front pointer. 
	ret_payload = q->array[q->front].payload;
	q->array[q->front].payload = NULL;
	q->front = ((q->front + 1) % q->maxSize);

	// unlock the mutex. 
	pthread_mutex_unlock(q->mutex);

	return ret_payload;
}

int queue_push(queue* q, void* new_payload){

	/* wait for free space on the queue*/
	// whates for freeSpace > 0
	while(sem_wait(q->freeSpace));	//--freeSpace

	// lock so that only one thread is signaling. 
	pthread_mutex_lock(q->mutex);

	// if contents was blocking a process because it was <0, wake the process. 
	sem_post(q->contents); //++contents, unblocks any blocked thread in queue_pop. 


	if(queue_is_full(q)){
		// this should not exicute. some shit when wrong. 
	   return QUEUE_FAILURE;
	}

	// add a new element to the circular array and relocate the rear pointer. 
	q->array[q->rear].payload = new_payload;
	q->rear = ((q->rear+1) % q->maxSize);

	// unblock so push can execute another thread.
	pthread_mutex_unlock(q->mutex);

	return QUEUE_SUCCESS;
}

void queue_cleanup(queue* q)
{
	while(!queue_is_empty(q)){
	   queue_pop(q);
	}

	free(q->array);
	sem_destroy(q->freeSpace);
	sem_destroy(q->contents);
	pthread_mutex_destroy(q->mutex);
}

void queue_close(queue* q)
{
	pthread_mutex_lock(q->mutex);
	q->finished = 1;	// stop the exicution of all poping threads
	if(queue_is_empty(q)){
		// Max number of threads
		while(1024)
			sem_post(q->contents);
	}
	pthread_mutex_unlock(q->mutex);
}
