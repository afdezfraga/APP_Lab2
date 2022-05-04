/*
   Programación Paralela Avanzada (Programación MPI Asíncrona)

   Alejandro Fernández Fraga

   mxvnm_MPI_Async.c

   Matrix-vector product. Matrix of N*M
*/

#include <mpi.h>

#include <stdio.h>
#include <stdlib.h>

#define ROOT_PROCESS 0

void distribute_data( int rank, int number_of_processes, 
                      int *N, int *M, int *n_local, float **x, 
                      int sendcounts[], int displs[],
                      float *Avector, float **A_local, float **y_local);


void gather_results(float *y, float *y_local, int n_local, 
                    int number_of_processes, int M, 
                    int recvcounts[], int displs[]);

int main(int argc, char *argv[])
{
  int i, j, N, M;
  float **A, *Avector, *x, *y, temp;

  int rank, number_of_processes;
  int n_local;
  float *A_local, *A_row, *y_local;
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &number_of_processes);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  int sendcounts[number_of_processes], displs[number_of_processes];

  if (rank == ROOT_PROCESS)
  {

    if (argc < 3)
    {
      fprintf(stderr, "Usar: %s filas columnas\n", argv[0]);
      MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    N = atoi(argv[1]); // Rows of the matrix and elements of vector y
    M = atoi(argv[2]); // Columns of the matrix and elements of vector x

    /* Allocate memory for global matrix */
    if ((Avector = (float *)malloc(N * M * sizeof(float))) == NULL)
      printf("Error en malloc Avector[%d]\n", N * M);

    /* Pointer array for addressing A as a matrix */
    if ((A = (float **)malloc(N * sizeof(float *))) == NULL)
      printf("Error en malloc del array de %d punteros\n", N);
    /* Asign to access A as a matrix: each pointer pointers to the start of the row */
    /* This way, all rows are consecutively in memory */
    for (i = 0; i < N; i++)
      *(A + i) = Avector + i * M;

    /* Matrix inicialization */
    for (i = 0; i < N; i++)
      for (j = 0; j < M; j++)
        A[i][j] = (0.15 * i - 0.1 * j) / N;

    /* Allocate memory for x and y vectors */
    if ((x = (float *)malloc(M * sizeof(float))) == NULL)
      printf("Error en malloc x[%d]\n", M);
    if ((y = (float *)malloc(N * sizeof(float))) == NULL)
      printf("Error en malloc y[%d]\n", N);

    /* x vector inicialization */
    for (i = 0; i < M; i++)
      x[i] = (M / 2.0 - i);
  }

  distribute_data(rank, number_of_processes, &N, &M, &n_local, 
                  &x, sendcounts, displs, Avector, &A_local, &y_local);

  /* Matrix-vector product, y = Ax */
  for (i = 0; i < n_local; i++)
  {
    A_row = A_local + i * M;
    temp = 0.0;
    for (j = 0; j < M; j++)
      temp += A_row[j] * x[j];
    y_local[i] = temp;
  }

  gather_results( y, y_local, n_local, number_of_processes, M, 
                  sendcounts, displs);

  if(rank == ROOT_PROCESS)
    printf("Done,  y[0] = %g  y[%d] = %g \n", y[0], N - 1, y[N - 1]);

  MPI_Finalize();
  return 0;
}


void distribute_data( int rank, int number_of_processes, 
                      int *N, int *M, int *n_local, float **x, 
                      int sendcounts[], int displs[],
                      float *Avector, float **A_local, float **y_local) {
  int n_resto;
  int n_div;
  MPI_Request requests[4];

  // Distribute parameters
  MPI_Ibcast( M, 1, MPI_INT, ROOT_PROCESS, MPI_COMM_WORLD, &(requests[0]));
  MPI_Ibcast( N, 1, MPI_INT, ROOT_PROCESS, MPI_COMM_WORLD, &(requests[1]));

  MPI_Wait( requests , MPI_STATUS_IGNORE);

  // Alloc vector x on non-root procs
  if (rank != ROOT_PROCESS){
    if ((*(x) = (float *)malloc(*(M) * sizeof(float))) == NULL)
      printf("Error en malloc x[%d]\n", *(M));
  }

  // Distribute vector x
  MPI_Ibcast( *(x), *M, MPI_FLOAT, ROOT_PROCESS, MPI_COMM_WORLD, &(requests[2]));

  MPI_Wait( &(requests[1]) , MPI_STATUS_IGNORE);

  // Init Scatterv parameters
  n_div = *(N) / number_of_processes;
  n_resto = *(N) % number_of_processes;
  if (rank == ROOT_PROCESS) {
    displs[0] = 0;
    for (int i = 0; i < number_of_processes - 1; i++)
    {
      sendcounts[i] = (i < n_resto) ? (n_div+1) * *(M) : n_div * *(M);
      displs[i+1] = displs[i] + sendcounts[i];
    }
    sendcounts[number_of_processes-1] = ((number_of_processes-1) < n_resto) ? (n_div+1) * *(M) : n_div * *(M);
  } else {
    sendcounts[rank] = (rank < n_resto) ? (n_div+1) * *(M) : n_div * *(M);
  }

  *n_local = (rank < n_resto) ? n_div+1 : n_div;

  // Alloc temporal vector y to store sub-vectors
  if ((*(y_local) = (float *)malloc(sendcounts[rank] / *(M) * sizeof(float))) == NULL)
    printf("Error en malloc y[%d]\n", *(N) );

  // Alloc sub-matrix A on all procs
  if ((*(A_local) = (float *)malloc(sendcounts[rank] * sizeof(float))) == NULL)
    printf("Error en malloc x[%d]\n", *(M) );

  // Distribute matrix Avector
  MPI_Iscatterv( Avector, sendcounts, displs, 
                MPI_FLOAT, *(A_local), sendcounts[rank], MPI_FLOAT, 
                ROOT_PROCESS, MPI_COMM_WORLD, &(requests[3]));

  MPI_Waitall( 2 , &(requests[2]) , MPI_STATUSES_IGNORE);

}

void gather_results(float *y, float *y_local, int n_local, 
                    int number_of_processes, int M, 
                    int recvcounts[], int displs[]) {

  MPI_Request request;

  for (int i = 0; i < number_of_processes; i++)
    {
      recvcounts[i] /= M;
      displs[i] /= M;
    }

  MPI_Igatherv(y_local, n_local, MPI_FLOAT, 
              y, recvcounts, displs, MPI_FLOAT, 
              ROOT_PROCESS, MPI_COMM_WORLD, &request);

  MPI_Wait( &request , MPI_STATUS_IGNORE);

}
