/*
   Programación Paralela Avanzada (Programación MPI Asíncrona)

   Alejandro Fernández Fraga

   dotprod_MPI_Async.c

   Dot product of two vectors
*/


#include <mpi.h>

#include <stdio.h>
#include <stdlib.h>

#define ROOT_PROCESS 0

int main(argc,argv)
int argc;
char *argv[];
{
  int N = 100000000, i;
  float *x, *y, *x_local, *y_local;
  float dot;
  MPI_Request requestB, requestR, requests[2];

  int rank, number_of_processes, n_local, n_resto;
  float dot_local;
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &number_of_processes);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  int sendcounts[number_of_processes], displs[number_of_processes];

  if (rank == ROOT_PROCESS)
  {
    if (argc < 2)
    {
      fprintf(stderr, "Use: %s num_elem_vector\n", argv[0]);
      MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    N = atoi(argv[1]);

    /* Allocate memory for vectors */
    if ((x = (float *)malloc(N * sizeof(float))) == NULL)
      printf("Error in malloc x[%d]\n", N);
    if ((y = (float *)malloc(N * sizeof(float))) == NULL)
      printf("Error in malloc y[%d]\n", N);

    /* Inicialization of x and y vectors*/
    for (i = 0; i < N; i++)
    {
      x[i] = (N / 2.0 - i);
      y[i] = 0.0001 * i;
    }

  }

  // Distribute vectors among processes
  MPI_Ibcast( &N, 1, MPI_INT, ROOT_PROCESS, MPI_COMM_WORLD, &requestB);
  MPI_Wait( &requestB , MPI_STATUS_IGNORE);

  // Init Scatterv parameters
  n_local = N / number_of_processes;
  n_resto = N % number_of_processes;
  if (rank == ROOT_PROCESS) {
    displs[0] = 0;
    for (int i = 0; i < number_of_processes - 1; i++)
    {
      sendcounts[i] = (i < n_resto) ? n_local+1 : n_local;
      displs[i+1] = displs[i] + sendcounts[i];
    }
    sendcounts[number_of_processes-1] = (number_of_processes-1 < n_resto) ? n_local+1 : n_local;
  } else {
    sendcounts[rank] = (rank < n_resto) ? n_local+1 : n_local;
  }

  /* Allocate memory for vectors */
  if ((x_local = (float *)malloc(sendcounts[rank] * sizeof(float))) == NULL)
    printf("Error in malloc x_local[%d]\n", sendcounts[rank]);
  if ((y_local = (float *)malloc(sendcounts[rank] * sizeof(float))) == NULL)
    printf("Error in malloc y_local[%d]\n", sendcounts[rank]);


  MPI_Iscatterv( x, sendcounts, displs, 
                  MPI_FLOAT, x_local, sendcounts[rank], MPI_FLOAT, 
                  ROOT_PROCESS, MPI_COMM_WORLD, &(requests[0]));

  MPI_Iscatterv( y, sendcounts, displs, 
                  MPI_FLOAT, y_local, sendcounts[rank], MPI_FLOAT, 
                  ROOT_PROCESS, MPI_COMM_WORLD, &(requests[1]));

  MPI_Waitall( 2 , requests , MPI_STATUSES_IGNORE);

  /* Dot product operation */

  dot_local = 0.;
  for (i = 0; i < sendcounts[rank]; i++)
    dot_local += x_local[i] * y_local[i];

  MPI_Ireduce( &dot_local, &dot, 
              1, MPI_FLOAT, 
              MPI_SUM, ROOT_PROCESS, MPI_COMM_WORLD, &requestR);
  MPI_Wait( &requestR , MPI_STATUS_IGNORE);

  if(rank == ROOT_PROCESS)
    printf("Dot product = %g\n", dot);


  MPI_Finalize();
  return 0;
}
