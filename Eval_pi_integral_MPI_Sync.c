/*
   Programación Paralela (MPI Básico)

   Alejandro Fernández Fraga
   Óscar Castellanos Rodríguez

   Eval_pi_integral_MPI.c

   Obtain the PI value using the numerical integration of 4/(1+x*x) between 0 and 1.
   PI/4=arctan(1)=integral 1/(1+x*x) , between 0 and 1.
   The numerical integration is calculated with n rectangular intervals (area=(1/n)*4/(1+x*x))) and adding the area of all these rectangle intervals  

   Programación Paralela Avanzada (Programación MPI Asíncrona)

   Alejandro Fernández Fraga

   Eval_pi_integral_MPI_Sync.c
*/
#include <mpi.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(argc,argv)
int argc;
char *argv[];
{
    int n; /* ONLY IN ROOT PROCESS*/ 
    int i;
    double PI25DT = 3.141592653589793238462643;
    double pi, h, sum, x;

    /* **************
    IMPLEMENT A PARALLEL VERSION OF THE CODE TO OBTAIN PARTIAL SOLUTION IN EACH PROCESSOR
	CHECK IF THE RESUTL OF THE PI VALUE
    ****************** */

    // MPI Init
    int myid, npes, n_local, start_local, end_local, n_resto;
    double pi_local;
    int MPI_ROOT_PROCESS = 0;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &npes);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);

    if(!myid)
        n = 1000000000;

    double t_max, t;
    MPI_Barrier(MPI_COMM_WORLD);
    t = MPI_Wtime();
    // Bcast N
    MPI_Bcast( &n , 1 , MPI_INT , MPI_ROOT_PROCESS , MPI_COMM_WORLD);

    // Init h, n_local, n_resto and sum
    h   = 1.0 / (double) n;
    sum = 0.0;
    n_local = n / npes;
    n_resto = n % npes;


    // For [myid*n_local, (myid+1*n_local)-1]
    // Do calculations
    if (myid < n_resto){
        start_local = myid * (n_local + 1); 
        end_local = (myid + 1) * (n_local + 1);
    } else {
        start_local = (myid * n_local) + n_resto;
        end_local = ((myid + 1) * n_local) + n_resto;
    }

    // pi_local = h * sum
    for (i = start_local; i < end_local; i ++) {
        x = h * ((double)i + 0.5);   //height of the rectangle
        sum += 4.0 / (1.0 + x*x);
    }

    pi_local = h * sum;

    // Allreduce pi_local -> pi
    MPI_Reduce( &pi_local, &pi, 1, MPI_DOUBLE, MPI_SUM, MPI_ROOT_PROCESS, MPI_COMM_WORLD);
    t = MPI_Wtime() - t ;
    MPI_Reduce( &t, &t_max, 1, MPI_DOUBLE, MPI_MAX, MPI_ROOT_PROCESS, MPI_COMM_WORLD);


    if (myid == MPI_ROOT_PROCESS){
        printf("The obtained Pi value is: %.16f, the error is: %.16f\n", pi, fabs(pi - PI25DT));
        printf("The parallel time is %lf\n", t_max);
    }

    MPI_Finalize();
}
