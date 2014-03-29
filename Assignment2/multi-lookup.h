/*
 * File: multi-lookup.h
 * Author: Brian Bauer <brian.bauer@colorado.edu>
 * Project: CSCI 3753 Programming Assignment 2
 * Create Date: 2014/02/20
 * Modify Date: 
 * Description:
 * 	
 * 
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include <semaphore.h>
#include "util.h"
#include "queue.h"

#define MINARGS 3
#define USAGE "<inputFilePath> <outputFilePath>"
#define SBUFSIZE 1025
#define INPUTFS "%1024s"
#define QSIZE 10
#define NUM_THREADS 5
#define TEST_SIZE 10

 void* requester(void* argv);

 void* resolver(void* outputfp);

