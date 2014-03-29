/*
 * File: mixed.c
 * Author: Sahle Alturaigi
 *  Some code from Andy Sayler
 * Project: CSCI 3753 Programming Assignment 3
 * Create Date: 2014/03/26
 * Description:
 * 	    This file contains a both the functionality of pi-sched.c
 *      and rw.c into one file. The purpose of this is to test
 *      how the different Linux schedules perform when burdened
 *      with both CPU intesive and I/O bound processes.
 */

 /*
 *  Dev notes: (please remove me later)
 *      argv[1] = policy type (use to be iterations. Didn't seem useful.)
 *      argv[2] = number of processes (use to be policy type.)
 *      argv[3] = Want output? (y = yes, n = no. Default is y)
 *
 */

/* Include Flags */
#define _GNU_SOURCE

/* Local Includes */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
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

/* Local Defines for pi-sched */
#define DEFAULT_ITERATIONS 1000000
#define RADIUS (RAND_MAX / 2)

/* Local Defines for rw */
#define MAXFILENAMELENGTH 80
#define DEFAULT_INPUTFILENAME "rwinput"
#define DEFAULT_OUTPUTFILENAMEBASE "rwoutput"
#define DEFAULT_BLOCKSIZE 1024
#define DEFAULT_TRANSFERSIZE 1024*100


inline double dist(double x0, double y0, double x1, double y1){
    return sqrt(pow((x1-x0),2) + pow((y1-y0),2));
}

inline double zeroDist(double x, double y){
    return dist(0, 0, x, y);
}

int main(int argc, char* argv[])
{

    /* My local variables */
    int max_processes;          // Max number of processes that will be generated
    int policy;                 // Policy to be used
    int status;                 // To check on child processes
    int show_results;           // y = show results. n = don't show
    long j = 0;                 // Control variable
    struct sched_param param;   // For scheduling policy
    pid_t pid, wpid;            // For managing forks

    /* pi-sched local variables */
    double x, y;
    double inCircle = 0.0;
    double inSquare = 0.0;
    double pCircle = 0.0;
    double piCalc = 0.0;
    long i;
    long iterations = DEFAULT_ITERATIONS;

    /* rw local variables */   
    int rv;
    int inputFD;
    int outputFD;
    char inputFilename[MAXFILENAMELENGTH];
    char outputFilename[MAXFILENAMELENGTH];
    char outputFilenameBase[MAXFILENAMELENGTH];

    ssize_t transfersize = DEFAULT_TRANSFERSIZE;
    ssize_t blocksize = DEFAULT_BLOCKSIZE; 
    char* transferBuffer = NULL;
    ssize_t buffersize;

    ssize_t bytesRead = 0;
    ssize_t totalBytesRead = 0;
    int     totalReads = 0;
    ssize_t bytesWritten = 0;
    ssize_t totalBytesWritten = 0;
    int     totalWrites = 0;
    int     inputFileResets = 0;

    /* Set default policy if not supplied */
    policy = SCHED_OTHER;

    /* Set default number of processes to use if not supplied */
	max_processes = LOW;

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

    /* Set default input and output files */
    strncpy(inputFilename, DEFAULT_INPUTFILENAME, MAXFILENAMELENGTH);
    strncpy(outputFilenameBase, DEFAULT_OUTPUTFILENAMEBASE, MAXFILENAMELENGTH);

    /* Set process to max prioty for given scheduler */
    param.sched_priority = sched_get_priority_max(policy);
    
    /* Set new scheduler policy */
    /*fprintf(stdout, "Current Scheduling Policy: %d\n", sched_getscheduler(0));
    fprintf(stdout, "Setting Scheduling Policy to: %d\n", policy);*/
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
            /*** BEGIN CPU Bound region***/
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

            /*** END CPU bound region ***/

            /*** BEGIN I/O bound region ***/
            /* Confirm blocksize is multiple of and less than transfersize*/
            if(blocksize > transfersize)
            {
                fprintf(stderr, "blocksize can not exceed transfersize\n");
                exit(EXIT_FAILURE);
            }
            if(transfersize % blocksize)
            {
                fprintf(stderr, "blocksize must be multiple of transfersize\n");
                exit(EXIT_FAILURE);
            }

            /* Allocate buffer space */
            buffersize = blocksize;
            if(!(transferBuffer = malloc(buffersize*sizeof(*transferBuffer))))
            {
                perror("Failed to allocate transfer buffer");
                exit(EXIT_FAILURE);
            }
        
            /* Open Input File Descriptor in Read Only mode */
            if((inputFD = open(inputFilename, O_RDONLY | O_SYNC)) < 0)
            {
                perror("Failed to open input file");
                exit(EXIT_FAILURE);
            }

            /* Open Output File Descriptor in Write Only mode with standard permissions*/
            rv = snprintf(outputFilename, MAXFILENAMELENGTH, "%s-%d", outputFilenameBase, getpid());  

            if(rv > MAXFILENAMELENGTH)
            {
                fprintf(stderr, "Output filenmae length exceeds limit of %d characters.\n", MAXFILENAMELENGTH);
                exit(EXIT_FAILURE);
            }
            else if(rv < 0)
            {
                perror("Failed to generate output filename");
                exit(EXIT_FAILURE);
            }
            if((outputFD =
                open(outputFilename,
                    O_WRONLY | O_CREAT | O_TRUNC | O_SYNC,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) < 0){
                perror("Failed to open output file");
                exit(EXIT_FAILURE);
            }

            /* Print Status */
            if(show_results)
            {
                fprintf(stdout, "Reading from %s and writing to %s\n", inputFilename, outputFilename);
            }
            /* Read from input file and write to output file*/
            do
            {
                /* Read transfersize bytes from input file*/
                bytesRead = read(inputFD, transferBuffer, buffersize);
                if(bytesRead < 0)
                {
                    perror("Error reading input file");
                    exit(EXIT_FAILURE);
                }
                else
                {
                    totalBytesRead += bytesRead;
                    totalReads++;
                }
        
            /* If all bytes were read, write to output file*/
            if(bytesRead == blocksize)
            {
                bytesWritten = write(outputFD, transferBuffer, bytesRead);
                if(bytesWritten < 0)
                {
                    perror("Error writing output file");
                    exit(EXIT_FAILURE);
                }
                else
                {
                    totalBytesWritten += bytesWritten;
                    totalWrites++;
                    }
                }
                /* Otherwise assume we have reached the end of the input file and reset */
                else
                {
                    if(lseek(inputFD, 0, SEEK_SET))
                    {
                        perror("Error resetting to beginning of file");
                        exit(EXIT_FAILURE);
                    }
                    inputFileResets++;
                }
            } while(totalBytesWritten < transfersize);

            /* Output some possibly helpfull info to make it seem like we were doing stuff */
            if(show_results)
            {
                fprintf(stdout, "Read:    %zd bytes in %d reads\n",
                    totalBytesRead, totalReads);
                fprintf(stdout, "Written: %zd bytes in %d writes\n",
                    totalBytesWritten, totalWrites);
                fprintf(stdout, "Read input file in %d pass%s\n",
                    (inputFileResets + 1), (inputFileResets ? "es" : ""));
                fprintf(stdout, "Processed %zd bytes in blocks of %zd bytes\n",
                    transfersize, blocksize);
            }
        
            /* Free Buffer */
            free(transferBuffer);

            /* Close Output File Descriptor */
            if(close(outputFD))
            {
                perror("Failed to close output file");
                exit(EXIT_FAILURE);
            }

            /* Close Input File Descriptor */
            if(close(inputFD))
            {
                perror("Failed to close input file");
                exit(EXIT_FAILURE);
            }
            /*** END I/O Bound region ***/

            // Terminate child process
            exit(0);
        }
    }

    /* Have parent wait until ALL child processes terminate */
    while((wpid = wait(&status)) > 0) 
    {
        // If terminate normally
        if(WIFEXITED(status))
            ++j;
    }

    printf("Number of processes terminated: %li\n", j);
    return 0;
}
