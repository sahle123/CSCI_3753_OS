/*  multi-lookup.c
*
*   Author: Sahle A. "Nomad" Alturaigi
*   Date:   Feb. 21, 2014
*   Completed Feb. 28, 2014
*   
*   Programming assignment 2 implementation file
*
*/

#include "multi-lookup.h"
#include "queue.h"
#include "util.h"

int Is_Requester_Threads_Done = 0;  // Bool to make sure resolver threads do not execute until
                                    // requester threads are done. (Idea taken from moodle)

int main(int argc, char* argv[])
{

  queue request_queue;                // The queue
  int check_mutex;                    // For creating mutex and catching errors
  pthread_mutex_t queue_mutex;        // mutex queue 
  pthread_mutex_t output_file_mutex;  // output file for mutex (CHECK)
  int input_file_size = argc-2;       // Array that holds the number of input files.
  FILE* inputfp[input_file_size];     // Array to hold all the input files
  FILE* outputfp = NULL;              // For results.txt
  FILE* current_file;                 // Temp for holding files in for loop.
  pthread_t requester_threads[input_file_size];     // Threads that will be sent to requester function
  pthread_t resolver_threads[MAX_RESOLVER_THREADS]; // Threads that will be sent to resolver function
  char errorstr[SBUFSIZE];            // Prints out name of file. For error checking.

  int i;  // Control variable
  int j;  // Control variable


  thread_info request_threads[input_file_size];       // Array to hold all request thread data
  thread_info resolve_threads[MAX_RESOLVER_THREADS];  // Array to hold all resolve thread data

  // Initiailize queue and check for errors
  if(queue_init(&request_queue, QUEUEMAXSIZE) == QUEUE_FAILURE)
    fprintf(stderr, "ERROR in queue_init\n");


  // Mutex for the request queue and output file. Checks for errors too.
  check_mutex = pthread_mutex_init(&queue_mutex, NULL);
  if(check_mutex)
    fprintf(stderr, "ERROR in mutex_init for queue in MAIN. Code: %d\n", check_mutex);
  check_mutex = pthread_mutex_init(&output_file_mutex, NULL);
  if(check_mutex)
    fprintf(stderr, "ERROR in mutex_init for output file in MAIN. Code: %d\n", check_mutex);


  // Check if there is a valid amount of arguments from the command prompt.
  if(argc < MINARGS)
  {
    fprintf(stderr, "Not enough arguments: %d\n", (argc-1));
    fprintf(stderr, "Usage:\n %s %s\n", argv[0], USAGE);
    return 1;
  }

  // Open file. If it fails, program exits
  outputfp = fopen(argv[(argc-1)], "w");
  if(!outputfp)
  {
    perror("Error opening output file. Aborting.");
    return 1;
  }

  // Iterate through the input files and store them in an array.
  for(i = 1; i < (argc-1); ++i)
  {
    inputfp[i-1] = fopen(argv[i], "r");
    if(!inputfp[i-1])
    {
      sprintf(errorstr, "Error opening input file %s in main.", argv[i]);
      perror(errorstr);
      break;
    }
  }


  printf("Creating requester threads...\n"); // Delete later

  // Create an equal amount of requester threads to input files.
  for(j = 0; j < input_file_size; ++j)
  {
    current_file = inputfp[j];
    request_threads[j].thread_file = current_file;
    request_threads[j].queue_mutex = &queue_mutex;
    request_threads[j].file_mutex = NULL;
    request_threads[j].request_queue = &request_queue;

    // Create threads to go into requester function. Checks for errors.
    // At this point, we have  the thread data wrapped up in the request_threads[] array. 
    int check_requester_threads;
    // We pass in request_threads as an argument for request_thread_pool_func.
    // requester_threads are the threads; request_threads are the values to be passed in.
    check_requester_threads = pthread_create(&(requester_threads[j]), 
      NULL, requester_thread_pool_func, &(request_threads[j]));
    if(check_requester_threads)
    {
      printf("ERROR in pthread_create in MAIN. Code: %d\n", check_requester_threads);
      exit(1); // Terminate process. (Which means all threads are terminated too)
    } 
  }


  printf("All requester threads successfully created.\n");
  printf("Creating resolver threads now...\n");

  // Storing resolver thread data then creating resolver threads to pass into 
  // resolver_thread_pool_func() 
  for(j = 0; j < MAX_RESOLVER_THREADS; ++j)
  {
    // Getting thread data and storing it to a temp.
    resolve_threads[j].thread_file = outputfp;
    resolve_threads[j].queue_mutex = &queue_mutex;
    resolve_threads[j].file_mutex = &output_file_mutex;
    resolve_threads[j].request_queue = &request_queue;

    // This is the same process as the requester thread, but we're sending the thread info
    // to resolver_thread_pool_func instead.
    int check_resolver_threads;
    check_resolver_threads = pthread_create(&(resolver_threads[j]),
      NULL, resolver_thread_pool_func, &(resolve_threads[j]));
    if(check_resolver_threads)
    {
      printf("ERROR in pthread_create in MAIN. Code: %d\n", check_resolver_threads);
      exit(1); // Terminate process. (Which means all threads are terminated too)
    }
  }


  // Waiting for all requester threads to terminate. 
  int check_pthread_join;
  for(j = 0; j < input_file_size; ++j)
  {
    check_pthread_join = pthread_join(requester_threads[j], NULL);
    if(check_pthread_join)
      fprintf(stderr,"ERROR in pthread_join in MAIN. Code: %d\n", check_pthread_join);
  }

  printf("Requester threads have completed successfully\n");
  Is_Requester_Threads_Done = 1;

  // Waiting for all resolver threads to terminate.
  for(j = 0; j < MAX_RESOLVER_THREADS; ++j)
  {
    check_pthread_join = pthread_join(resolver_threads[j], NULL);
    if(check_pthread_join)
      fprintf(stderr, "ERROR in pthread_join in MAIN. Code: %d\n", check_pthread_join);
  }

  printf("Resolver threads have completed successfully\n");

  /* Freeing up all used heap space */
  // Close file
  if(fclose(outputfp))
  {
    perror("Error closing output file.\n");
    return 1;
  }

  // Free up queue so we don't fail valgrind test.
  queue_cleanup(&request_queue);

  // Free up space from mutex queue.
  check_mutex = pthread_mutex_destroy(&queue_mutex);
  if(check_mutex)
    fprintf(stderr, "ERROR in pthread_mutex_destory in MAIN. Code: %d\n", check_mutex);

  // Free up space from mutex file.
  check_mutex = pthread_mutex_destroy(&output_file_mutex);
  if(check_mutex)
    fprintf(stderr, "ERROR in pthread_mutex_destory in MAIN. Code: %d\n", check_mutex);


  return 0;
}
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void* requester_thread_pool_func(void* thread_data)
{
  // Struct pointer for the thread struct in parameter list.
  thread_info* thread_info = thread_data; // May change name.

  // Extracting all the thread_data into local pointers
  FILE* requester_thread_file = thread_info->thread_file;   // File
  pthread_mutex_t* queue_mutex = thread_info->queue_mutex; // queue_mutex
  queue* req_queue = thread_info->request_queue;            // request_queue

  char hostname[SBUFSIZE];              // Initialize Hostname array.
  char* payload;                        // Initialize a pointer to the Payload. (i.e. The URL from the input file)
  int hostname_pushed_to_queue = 0;     // A bool that will take care of pushing hostname[] to queue.
  // To check for errors in lock or unlock for mutex.
  int mutex_lock_error;             
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
        fprintf(stderr, "ERROR in requester_thread_pool_func mutex_lock. Code: %d\n", mutex_lock_error);

      // If the queue is full, thread will go to sleep
      if(queue_is_full(req_queue))
      {
        // Unlock the queue
        mutex_unlock_error = pthread_mutex_unlock(queue_mutex);
        if(mutex_unlock_error)
        {
          fprintf(stderr, "ERROR in requester_thread_pool_func mutex_unlock. Code: %d\n", mutex_unlock_error);
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
          fprintf(stderr, "ERROR in requester_thread_pool_func queue_push.\n");

        // Unlock queue
        mutex_unlock_error = pthread_mutex_unlock(queue_mutex);
        if(mutex_unlock_error)
          fprintf(stderr, "ERROR in requester_thread_pool_func mutex_unlock 2. Code: %d\n", mutex_unlock_error);

        // Hostname has been pushed to queue. :)
        hostname_pushed_to_queue = 1;
      }
    }

    // Reset pushed_to_queue status back to false and go to next hostname.
    hostname_pushed_to_queue = 0;
  }

  // Close input file and end function call.
  if(fclose(requester_thread_file))
    perror("Error CLosing Input file\n");

  return NULL;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void* resolver_thread_pool_func(void* thread_data)
{
  // Struct pointer for the thread struct in the parameter list.
  thread_info* thread_info = thread_data;

  // extracting all thread data into local pointers
  FILE* resolver_thread_file = thread_info->thread_file;
  pthread_mutex_t* queue_mutex = thread_info->queue_mutex;
  pthread_mutex_t* file_mutex = thread_info->file_mutex;
  queue* req_queue = thread_info->request_queue;
  char ipstr[INET6_ADDRSTRLEN];  // String to hold the IP addresses 
  char* payload;                 // Initialize a pointer to the Payload. (i.e. The URL from the input file)
  // TO check for errors in lock or unlock for mutex.
  int mutex_lock_error;
  int mutex_unlock_error;

  /*
  *   Handle empty queues and queues that have content.
  */
  while(!queue_is_empty(req_queue) || !Is_Requester_Threads_Done)
  {
    // Lock queue so only the running thread can access it.
    mutex_lock_error = pthread_mutex_lock(queue_mutex);
    if(mutex_lock_error)
      fprintf(stderr, "ERROR in pthread_mutex_lock in resolver_thread_pool_func. Code: %d\n", mutex_lock_error);

    // Retrieve hostname
    payload = queue_pop(req_queue);

    // Handle empty queue. (Have it sleep)
    if(payload == NULL)
    {
      mutex_unlock_error = pthread_mutex_unlock(queue_mutex);
      if(mutex_unlock_error)
        fprintf(stderr, "ERROR in pthread_mutex_lock in resolver_thread_poolfunc. Code: %d\n", mutex_unlock_error);

      printf("Queue is empty... Waiting on threads.\n");
      usleep(100);
    }
    else
    {
      mutex_unlock_error = pthread_mutex_unlock(queue_mutex);
      if(mutex_unlock_error)
        fprintf(stderr, "ERROR in pthread_mutex_lock in resolver_thread_pool_func. Code: %d\n", mutex_unlock_error);

      // Lookup hostname and store IP string into ipstr
      if(dnslookup(payload, ipstr, sizeof(ipstr)) == UTIL_FAILURE)
      {
        fprintf(stderr, "dnslookup error: %s\n", payload);
        strncpy(ipstr, "", sizeof(ipstr));
      }

      // Lock output file
      mutex_lock_error = pthread_mutex_lock(file_mutex);
      if(mutex_lock_error)
        fprintf(stderr, "ERROR in pthread_mutex_lock in resolver_thread_pool_func. Code: %d\n", mutex_lock_error);

      // Write onto output file 
      fprintf(resolver_thread_file, "%s, %s\n", payload, ipstr);

      // Unlock output file
      mutex_unlock_error = pthread_mutex_unlock(file_mutex);
      if(mutex_unlock_error)
        fprintf(stderr, "ERROR in pthread_mutex_lock in resolver_thread_pool_func. Code: %d\n", mutex_lock_error);

      // Free up memory from heap.
      free(payload);
    }
  }

  return NULL;

}