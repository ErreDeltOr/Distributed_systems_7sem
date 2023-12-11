/* Include benchmark-specific header. */
#include "3mm.h"
#include "mpi.h"
#include <mpi-ext.h>
#include <signal.h>
#include <stdio.h>

double bench_t_start, bench_t_end;

static
double rtclock()
{
    struct timeval Tp;
    int stat;
    stat = gettimeofday (&Tp, NULL);
    if (stat != 0)
      printf ("Error return from gettimeofday: %d", stat);
    return (Tp.tv_sec + Tp.tv_usec * 1.0e-6);
}

void bench_timer_start()
{
  bench_t_start = rtclock ();
}

void bench_timer_stop()
{
  bench_t_end = rtclock ();
}

void bench_timer_print()
{
  printf ("Time in seconds = %0.6lf\n", bench_t_end - bench_t_start);
}

static
void init_array(int ni, int nj, int nk, int nl, int nm,
  double A[ ni][nk],
  double B[ nk][nj],
  double C[ nj][nm],
  double D[ nm][nl])
{
  int i, j;

  for (i = 0; i < ni; i++)
    for (j = 0; j < nk; j++)
      A[i][j] = (double) ((i*j+1) % ni) / (5*ni);
  for (i = 0; i < nk; i++)
    for (j = 0; j < nj; j++)
      B[i][j] = (double) ((i*(j+1)+2) % nj) / (5*nj);
  for (i = 0; i < nj; i++)
    for (j = 0; j < nm; j++)
      C[i][j] = (double) (i*(j+3) % nl) / (5*nl);
  for (i = 0; i < nm; i++)
    for (j = 0; j < nl; j++)
      D[i][j] = (double) ((i*(j+2)+2) % nk) / (5*nk);
}



static
void print_array(int ni, int nl,
   double G[ ni][nl])
{
  int i, j;

  fprintf(stdout, "==BEGIN DUMP_ARRAYS==\n");
  fprintf(stdout, "begin dump: %s", "G");
  for (i = 0; i < ni; i++)
    for (j = 0; j < nl; j++) {
 if ((i * ni + j) % 20 == 0) fprintf (stderr, "\n");
 fprintf (stdout, "%0.2lf ", G[i][j]);
    }
  fprintf(stdout, "\nend   dump: %s\n", "G");
  fprintf(stdout, "==END   DUMP_ARRAYS==\n");
}

MPI_Comm main_comm;
int my_rank;
int comm_sz;

static
void mult(int comm_sz, int my_rank, int ni, int nk, int nj,
  double E[ ni][nj],
  double A[ ni][nk],
  double B[ nk][nj])
{
    int i,j,k;
    int local_M;
    local_M = ni/comm_sz;
    double (*local_Matrix_one)[local_M][nk]; local_Matrix_one = (double(*)[local_M][nk])malloc ((local_M) * (nk) * sizeof(double));
    double (*local_result)[local_M][nj]; local_result = (double(*)[local_M][nj])malloc ((local_M) * (nj) * sizeof(double));
    MPI_Scatter(A, local_M * nk,MPI_DOUBLE, *local_Matrix_one,local_M * nk,MPI_DOUBLE,0,MPI_COMM_WORLD);
    MPI_Bcast(B, nk*nj, MPI_DOUBLE,0, MPI_COMM_WORLD);
    for(i=0;i<local_M;i++)
    for(j=0;j<nj;j++){
        (*local_result)[i][j] = 0.0;
    for(k=0;k<nk;k++)
    (*local_result)[i][j] +=(*local_Matrix_one)[i][k]*B[k][j];
    }
    free((void *)local_Matrix_one);

    MPI_Gather(*local_result,local_M*nj,MPI_DOUBLE,E,local_M*nj,MPI_DOUBLE,0,MPI_COMM_WORLD);
    if (my_rank == 0) {
    int rest = ni%comm_sz;
    if (rest!=0)
    for(i=ni-rest-1;i<ni;i++)
    for(j=0;j<nj;j++){
    E[i][j] = 0.0;
    for(k=0;k<nk;k++)
    E[i][j]+=A[i][k]*B[k][j];
    }
    }
    free((void*)local_result);
}

static void errhandler(MPI_Comm *comm, int *err, ...) {
    int len;
    char errstr[MPI_MAX_ERROR_STRING];
    MPI_Comm_rank(main_comm, &my_rank);
    MPI_Comm_size(main_comm, &comm_sz);
    MPI_Error_string(*err, errstr, &len);
    fprintf(stderr, "Rank %d / %d: notified of error %s\n", my_rank, comm_sz, errstr);

    MPIX_Comm_shrink(main_comm, &main_comm);
    MPI_Comm_rank(main_comm, &my_rank);
    MPI_Comm_size(main_comm, &comm_sz);
}

static
void kernel_3mm(int ni, int nj, int nk, int nl, int nm,
  double E[ ni][nj],
  double A[ ni][nk],
  double B[ nk][nj],
  double F[ nj][nl],
  double C[ nj][nm],
  double D[ nm][nl],
  double G[ ni][nl])
{
    int err_code = 0;
    
    MPI_Init(NULL,NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    
    main_comm = MPI_COMM_WORLD;
    
    MPI_Errhandler errh;
    MPI_Comm_create_errhandler(errhandler, &errh);
    MPI_Comm_set_errhandler(main_comm, errh);
    MPI_Barrier(main_comm);
    
    if (my_rank == 2) {
        fprintf(stdout, "Process %d died\n", my_rank);
        raise(SIGKILL);
    }

checkpoint:
    if (my_rank==0)
        bench_timer_start();
    mult(comm_sz, my_rank, ni, nk, nj, E, A, B);
    if (err_code != 0) {
        printf("%d processes alive\n", comm_sz);
        goto checkpoint;
    }
    mult(comm_sz, my_rank, nj, nm, nl, F, C, D);
    err_code = MPI_Barrier(main_comm);
    if (err_code != 0) {
        printf("%d processes alive\n", comm_sz);
        goto checkpoint;
    }
    mult(comm_sz, my_rank, ni, nj, nl, G, E, F);
    err_code = MPI_Barrier(main_comm);
    if (err_code != 0) {
        printf("%d processes alive\n", comm_sz);
        goto checkpoint;
    }
    MPI_Finalize();
    
    if (my_rank==0) {
        bench_timer_stop();
        bench_timer_print();
    }
}

int main(int argc, char** argv)
{
  int ni = NI;
  int nj = NJ;
  int nk = NK;
  int nl = NL;
  int nm = NM;

  double (*E)[ni][nj]; E = (double(*)[ni][nj])malloc ((ni) * (nj) * sizeof(double));
  double (*A)[ni][nk]; A = (double(*)[ni][nk])malloc ((ni) * (nk) * sizeof(double));
  double (*B)[nk][nj]; B = (double(*)[nk][nj])malloc ((nk) * (nj) * sizeof(double));
  double (*F)[nj][nl]; F = (double(*)[nj][nl])malloc ((nj) * (nl) * sizeof(double));
  double (*C)[nj][nm]; C = (double(*)[nj][nm])malloc ((nj) * (nm) * sizeof(double));
  double (*D)[nm][nl]; D = (double(*)[nm][nl])malloc ((nm) * (nl) * sizeof(double));
  double (*G)[ni][nl]; G = (double(*)[ni][nl])malloc ((ni) * (nl) * sizeof(double));
  

  init_array(ni, nj, nk, nl, nm,
       *A,
       *B,
       *C,
       *D);

  kernel_3mm (ni, nj, nk, nl, nm,
              *E,
              *A,
              *B,
              *F,
              *C,
              *D,
              *G);


  if (argc > 42 && ! strcmp(argv[0], "")) print_array(ni, nl, *G);
  free((void*)E);
  free((void*)A);
  free((void*)B);
  free((void*)F);
  free((void*)C);
  free((void*)D);
  free((void*)G);

  return 0;
}

