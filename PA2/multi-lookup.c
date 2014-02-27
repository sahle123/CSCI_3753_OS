/*	multi-lookup.c
*
*	Author: Sahle A. "Nomad" Alturaigi
*	Date:   Feb. 21, 2014
*	
*	Programming assignment 2 implementation file
*
*/

//** Directives **//
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h> // POSIX OS API (read, write, etc...)

#include "multi-lookup.h"
#include "queue.h"
#include "util.h"

int main(int argc, char* argv[])
{
  return 0;
}
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void* requester_thread_function(void* thread_information)
{
  // Struct pointer for the thread struct in parameter list.
  struct thread_info* thread_info = thread_information; // May change name.

  // Extracting all the thread_information into local pointers
  FILE* requester_thread_file = thread_info->thread_file;   /// File
  phthread_mutex_t* queue_mutex = thread_info->queue_mutex; /// queue_mutex
  queue* req_queue = thread_info->request_queue;            /// request_queue

  // Initialize Hostname array. Empty still...
  char hostname[SBUFSIZE];

  // Initialize a pointer to the Payload. (i.e. The URL from the input file)
  char* payload;

  // A variable that will take care of pushing hostname[] to queue.
  int hostname_pushed_to_queue = 0;

  // To check for errors.
  int mutex_lock_error
  int mutex_unlock_error;

  /*
  *   Read in file to program 
  */
  while(fscanf(requester_thread_file, INPUTFS, hostname) > 0)
  {
    // Loop will keep going until hostname is on queue
    while(!hostname_pushed_to_queue)
    {
      // Lock queue so only the currently running thread has access to the buffer.
      mutex_lock_error = pthread_mutex_lock(queue_mutex);

      // Error handler for mutex_lock. Should be 0 if all is well.
      if(mutex_lock_error)
        fprintf(stderr, "ERROR in requester_thread_function mutex_lock. 
          Code: %d\n", mutex_lock_error);

      // If the queue is full, thread will go to sleep
      if(queue_is_full(req_queue))
      {
        // Unlock the queue
        mutex_unlock_error = pthread_mutex_unlock(queue_mutex);
        if(mutex_unlock_error)
        {
          fprintf(stderr, "ERROR in requester_thread_function mutex_unlock. 
            Code: %d\n", mutex_unlock_error);
        }

        // Taken from unistd.h. This makes the thread sleep for 0.1 seconds.
        usleep(100);
      }
      // If the queue is NOT full. We put thread onto queue.
      else
      {
        // Initializing the payload size
        payload = malloc(SBUFSIZE);

        // Copying all the hostnames from the input file into the payload
        strncpy(payload, hostname, SBUFSIZE);

        // Push thread onto queue and check for any errors.
        if(queue_push(req_queue, payload) == QUEUE_FAILURE)
          fprintf(stderr, "ERROR in requester_thread_function queue_push.\n");

        // Unlock queue
        mutex_unlock_error = pthread_mutex_unlock(queue_mutex);
        if(mutex_unlock_error)
          fprintf(stderr, "ERROR in requester_thread_function mutex_unlock 2.
            Code: %d\n", mutex_unlock_error);

        // Hostname has been pushed to queue. :)
        hostname_pushed_to_queue = 1;
      }
    }

    // Reset pushed_to_queue status back to false and go to next hostname.
    hostname_pushed_to_queue = 0;
  }

  // Close input file and end function call.
  if(fclose(requester_thread_file))
      prerror("Error CLosing Input file");

  return NULL;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void* resolver_thread_function(void* thread_information)
{

}