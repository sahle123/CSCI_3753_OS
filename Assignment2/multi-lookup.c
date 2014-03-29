/*
 * File: multi-lookup.c
 * Author: Jacob Rail <jacot.rail@colorado.edu>
 * Project: CSCI 3753 Programming Assignment 2
 * Create Date: 2014/02/20
 * Modify Date: 2014/02/28
 * Description:
 * 	This file contains the threaded solution
 *      to this assignment.
 */


/**********************************   Main   ***********************************/
#include "multi-lookup.h"
#define TRUE 1
#define FALSE 0

queue inputQueue;

int requester_running = TRUE;


int main(int argc, char* argv[]){
	// open output file. 
	FILE* outputfp;
	outputfp = fopen(argv[argc-1], "w");
	if(!outputfp){
		perror("Error Opening Output File");    
		return EXIT_FAILURE;
	}

	/* Create two thread pools for five threads each for a total of 10 threads. */
	pthread_t requesterThreads[NUM_THREADS];
	pthread_t resolverThreads[NUM_THREADS];
	/* Declare local vars */
	int rc;
	long t;

	/* Initialize input file queue */
	if (queue_init(&inputQueue, QSIZE) == QUEUE_FAILURE){
		fprintf(stderr, "fileQ failed to initialize \n");
	}

	/* Create a requester thread pool for reading in the files */
	/* creates 5 threads that start exicuting the requester function. */
	/* one thread for each input file. */
	for(t=0; t<NUM_THREADS; t++){
		printf("In main: creating requester thread %ld\n", t);
		rc = pthread_create(&(requesterThreads[t]), NULL, requester, argv[t+1]);
		if (rc){
			printf("ERROR: return code from pthread_create() is %d\n", rc);
			exit(EXIT_FAILURE);
		}
	}
	// Create a resolver pool for performing DNS calls from each line
	/* Create 5 threads that exicute the resolver funciton. */
	/* The threads constantly try to pop from the queue */
	for(t=0;t<NUM_THREADS;t++){
		printf("In main: creating resolver thread %ld\n", t);
		rc = pthread_create(&(resolverThreads[t]), NULL, resolver, outputfp);
		if (rc){
			printf("ERROR: return code from pthread_create() is %d\n", rc);
			exit(EXIT_FAILURE);
		}
	}

	/* Wait for All Theads to Finish */
	for(t=0;t<NUM_THREADS;t++){
		// waits for the given requester thread to terminate. 
		// a requester thread terminates when all of the hostnames are pushed onto the queue. 
		pthread_join(requesterThreads[t],NULL);
	}
	printf("All of the requester threads were completed!\n");
	
	requester_running = FALSE;
	queue_close(&inputQueue);	// this sets the queue finished flage = 1

	for(t=0;t<NUM_THREADS;t++){
		// waits for the given resolver thread to terminate. 
		// a resolver thread terminates when queue_pop returns NULL or when the queue is empty and requester_running is FALSE
		// requester_running is set to FALSE before this code exicutes.  
		pthread_join(resolverThreads[t],NULL);
	}
	printf("All of the resolver threads were completed!\n");

	queue_cleanup(&inputQueue);
	fclose(outputfp);

	return EXIT_SUCCESS;
}

/*******************************************************************************/
/*****************************   Functions   ***********************************/

/** Requester Thread 
 ---------------------
 *  The requester thread function and is called initializing the 
	requesterThreads. 
 *  This fuction is passes as an argument to pthread_create as a *void 
	(void pointer). 
 *  It is passes in as the starting function for pthread_create. 
 *  This function is the starting point for the requester threads.
 *  Only called once at the beggining as the third parameter to pthread_create. 
 */
 void* requester(void* argv)
 {
	char* fileName = argv;
	char hostname[SBUFSIZE];
	char* temp;
	FILE* inputfp = fopen(fileName, "r");	// reads in the file

	/* read in the hostnames and stores them on the queue. */
	/* continue to read in lines until the end of the file is reached. */
	while(fscanf(inputfp, INPUTFS, hostname) > 0)
	{												
		temp = malloc(SBUFSIZE);					// Allocate space to store the hostname. 
		strncpy(temp, hostname, SBUFSIZE);			// Put the hostname into temp (temp = hostname).
		queue_push(&inputQueue, temp);				// Push the hostname onto the queue. 
	}

	// close the file. 
	fclose(inputfp);

	return EXIT_SUCCESS;
 }

//-----------------------------------------------------------------------------//
/** Resolver Thread 
 --------------------
 *  The resolver function is similar to the requester function except it is used
	as the starting function for the creating requester threads, not resolver 
	threads.
 */

 void* resolver(void* outputfp)
 {
 	/* there is not SBUFSIZE for the resolver becuase hostname is set equil to 
 	 * the value popped from the queue, which is a *void. hostname[SBUFSIZE] 
 	 * would throw an error because the string popped from the queue could be
 	 * longer in length than SBUFSIZE. Therefor we have the hostname be a *char. */
	char* hostname;
	/* Array to store the IP address string. It is long enought to store the 
	 * address which is INET6_ADDRSTRLEN long. */
	char firstipstr[INET6_ADDRSTRLEN]; 

	// all of the threads will keep getting IP addresses untill the queue is empty. 
	// If the queue
	while(!queue_is_empty(&inputQueue) || requester_running)
	{
		hostname = queue_pop(&inputQueue);
		// popping a NULL from the queue means that the queue is empty. 
		if(hostname == NULL)
			return EXIT_SUCCESS;
		if(dnslookup(hostname, firstipstr, sizeof(firstipstr)) == UTIL_FAILURE){
			fprintf(stderr, "dnslookup error: %s\n", hostname);
			strncpy(firstipstr, "", sizeof(firstipstr));
		}
		fprintf(outputfp, "%s,%s\n", hostname, firstipstr);
		free(hostname);
	}
	return EXIT_SUCCESS;
 }

 /*******************************************************************************/
 /*******************************************************************************/


