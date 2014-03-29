/*
 * File: pi-sched.c
 * Author: Andy Sayler
 *      Modified by: Sahle Alturaigi
 * Project: CSCI 3753 Programming Assignment 3
 * Create Date: 2012/03/07
 * Modify Date: 2014/03/26
 * Description:
 * 	    This file contains a simple program for statistically
 *      calculating pi using a specific scheduling policy
 *      and a specific number of processes.
 */

 /*
 *  Dev notes: (please remove me later)
 *      argv[1] = policy type (use to be iterations. Didn't seem useful.)
 *      argv[2] = number of processes (use to be policy type.)
 *      argv[3] = Want output? (y = yes, n = no. Default is y)
 *
 *      Removed iterations from program. Iterations will always equal DEFAULT_ITERATIONS
 *      *Thinking of contracting scheduling policy names
 *            
 *
 */

/* Local Includes */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <sched.h>  // Lookup in detail

 // For Forking functionality
 #include <sys/types.h>
 #include <unistd.h>
 #include <sys/wait.h>

 /* Number of process instances */
#define LOW 10
#define MEDIUM 50
#define HIGH 100

#define DEFAULT_ITERATIONS 1000000
#define RADIUS (RAND_MAX / 2)

inline double dist(double x0, double y0, double x1, double y1){
    return sqrt(pow((x1-x0),2) + pow((y1-y0),2));
}

inline double zeroDist(double x, double y){
    return dist(0, 0, x, y);
}

int main(int argc, char* argv[]){

    int policy;
    int max_processes;  // C'est pour moi :)
    int status;         // C'est pour moi
    int show_results;   // y = show results. n = don't show
    double x, y;
    double inCircle = 0.0;
    double inSquare = 0.0;
    double pCircle = 0.0;
    double piCalc = 0.0;
    long i;
    long iterations = DEFAULT_ITERATIONS;
    pid_t pid, wpid;
    struct sched_param param;


    /* Set default policy if not supplied */
    policy = SCHED_OTHER;

    /* Set default number of processes to use if not supplied */
	max_processes = MEDIUM;

    /* Set default value for show_results */
    show_results = 0;

    
    /* Set policy if supplied */
    if(argc > 1)
    {
        if(!strcmp(argv[1], "SCHED_OTHER"))
	       policy = SCHED_OTHER;

        else if(!strcmp(argv[1], "SCHED_FIFO"))
	        policy = SCHED_FIFO;

        else if(!strcmp(argv[1], "SCHED_RR"))
	       policy = SCHED_RR;

        else{
	       fprintf(stderr, "Unhandled scheduling policy\n");
	       exit(EXIT_FAILURE);
        }
    }

    /* Set number of processes if supplied */
    if(argc > 2)
    {
        if(!strcmp(argv[2], "LOW"))
            max_processes = LOW;

        else if (!strcmp(argv[2], "MEDIUM"))
            max_processes = MEDIUM;

        else if (!strcmp(argv[2], "HIGH"))
            max_processes = HIGH;

        else{
            fprintf(stderr, "Invalid number of processes for argument 2, exiting...\n");
            exit(EXIT_FAILURE);
        }   
    }

    /* Check if user wants to show results if supplied */
    if(argc > 3)
    {
        if(!strcmp(argv[3], "y"))
            show_results = 1;

        else if(!strcmp(argv[3], "n"))
            show_results = 0;

        else{
            fprintf(stderr, "Can only choose y or n for argument 3, exiting...\n");
            exit(EXIT_FAILURE);
        }
    }

    /*** Switch statement refactor ***/
    /*if(argc > 1){
        switch(argv[1])
        {
            case "SCHED_OTHER" : 
                policy = SCHED_OTHER;
                break;
            case "SCHED_RR" :
                policy = SCHED_RR;
                break;
            case "SCHED_FIFO" :
                policy = SCHED_FIFO;
                break;
            default :
                fprintf(stderr, "Unhandled scheduling policy\n");
                exit(EXIT_FAILURE);
        }
    }

    if(argc > 2)
    {
        switch(argv[2])
        {
            case "LOW" :
                max_processes = LOW;
                break;
            case "MEDIUM" :
                max_processes = MEDIUM;
                break;
            case "HIGH" :
                max_processes = HIGH;
                break;
            default :
                fprintf(stderr, "Unhandled number of processes\n");
                exit(EXIT_FAILURE);

        }
    }

    if(argc > 3)
    {
        switch(argv[3])
        {
            case 'y' :
                show_results = 1;
                break;
            case 'n' :
                show_results = 0;
                break;
            default :
                fprintf(stderr, "Unhandled value\n");
                exit(EXIT_FAILURE);
        }
    }*/

    /* Set process to max prioty for given scheduler */
    param.sched_priority = sched_get_priority_max(policy);
    
    /* Set new scheduler policy */
    fprintf(stdout, "Current Scheduling Policy: %d\n", sched_getscheduler(0));
    fprintf(stdout, "Setting Scheduling Policy to: %d\n", policy);
    if(sched_setscheduler(0, policy, &param)){
	   perror("Error setting scheduler policy");
	   exit(EXIT_FAILURE);
    }
    fprintf(stdout, "New Scheduling Policy: %d\n", sched_getscheduler(0));

    /* Forking!!! REAP THE ZOMBIE CHILDREN!! */
    printf("\nForking %d processes now\n", max_processes);
    for(i = 0; i < max_processes; ++i)
    {
        pid = fork();   // Fork process

        /* If fork fails */
        if(pid == -1)
        {
            fprintf(stderr, "Error in Forking, exiting...");
            exit(EXIT_SUCCESS);
        }

        /* Child Process */
        if(pid == 0)
        {
            /* Calculate pi using statistical method across all iterations*/
            for(i = 0; i < iterations; i++)
            {
                x = (random() % (RADIUS * 2)) - RADIUS;
                y = (random() % (RADIUS * 2)) - RADIUS;
                if(zeroDist(x,y) < RADIUS)
                {
                    inCircle++;
                }
                inSquare++;
            }

            /* Finish calculation */
            pCircle = inCircle/inSquare;
            piCalc = pCircle * 4.0;

            /* Print result */
            if(show_results)
                fprintf(stdout, "pi = %f\n", piCalc);

            // Terminate child process
            exit(0);
        }
    }

    /* Reset i to count number of terminated processes */
    i = 0;

    /* Have parent wait until ALL child processes terminate */
    while((wpid = wait(&status)) > 0) 
    {
        // If terminate normally
        if(WIFEXITED(status))
            ++i;
    }

    printf("Number of processes terminated: %li\n", i);
    return 0;
}
