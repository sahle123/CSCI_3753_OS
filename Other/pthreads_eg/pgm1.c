#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

void *odd(void *max)
{
  int i;
  FILE *fd;

  fd = fopen("odd_num", "w");
  for (i = 1; i < *(int *)max; i+= 2)
    fprintf(fd, "%d\n", i);
}

void *even(void *max)
{
  int i;
  FILE *fd;
 
  fd = fopen("even_num", "w");
  for (i = 2; i < *(int *)max; i+= 2)
    fprintf(fd, "%d\n", i);
}

main()
{
  int max = 50, max1 = 100, max2 = 200, i;
  FILE *fd;
  pthread_t *th1, *th2;

  th1 = (pthread_t *) malloc(sizeof(pthread_t));
  if (pthread_create(th1, NULL, odd, (void *)&max1)) {
    perror("error creating the first thread");
    exit(1);
  }
  printf("created the first thread\n");

  th2 = (pthread_t *) malloc(sizeof(pthread_t));
  //th2 = new pthread_t();
  if (pthread_create(th2, NULL, even, (void *)&max2)) {
    perror("error creating the second thread");
    exit(1);
  }
  printf("created the first thread\n");

  fd = fopen("whole_num", "w");
  for (i = 1; i < max; i++)
    fprintf(fd, "%d\n", i);
  pthread_join(*th1, 0);
  pthread_join(*th2, 0);
}
