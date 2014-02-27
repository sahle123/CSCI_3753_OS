/*	multi-lookup.h
*
*	Author: Sahle A. "Nomad" Alturaigi
*	Date:   Feb. 21, 2014
*	
*	Programming assignment 2 header file
*
*/

#ifndef MULTI_LOOKUP_H
#define MULTI_LOOKUP_H

//** Directives **//
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h> // POSIX OS API (read, write, etc...)
//#include "queue.h" 

// Constants from lookup.c Tentative. May change...
#define MINARGS 3
#define USAGE "<inputFilePath> <outputFilePath>"
#define SBUFSIZE 1025
#define INPUTFS "%1024s"
#define MAX_RESOLVER_THREADS 10 /// May change size later...

/*
*	Struct that will be used for request and resolver functions below.
*	This structure will let us hold all the info of the thread we need
*	info a single data structure we can easily pass into functions.
*/
struct thread_info {
	FILE* thread_file;
	pthread_mutex_t* queue_mutex;
	pthread_mutex_t* file_mutex;
	queue* request_queue;
}

/* 
*	Functions that will handle the two types of threads we're dealing with
*/
void* requester_thread_function(void* thread_information);
void* resolver_thread_function(void* thead_information);

#endif // MULTI_LOOKUP_H