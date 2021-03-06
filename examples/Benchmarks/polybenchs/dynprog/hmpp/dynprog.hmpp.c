#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include "timing.h"

/* Default problem size. */
#ifndef TSTEPS
# define TSTEPS 10000
#endif
#ifndef LENGTH
# define LENGTH 50
#endif

/* Default data type is int. */
#ifndef DATA_TYPE
# define DATA_TYPE int
#endif
#ifndef DATA_PRINTF_MODIFIER
# define DATA_PRINTF_MODIFIER "%d "
#endif

DATA_TYPE out;
DATA_TYPE sum_c[LENGTH][LENGTH][LENGTH];
DATA_TYPE c[LENGTH][LENGTH];
DATA_TYPE W[LENGTH][LENGTH]; //input

static inline
void init_array() {
  int i, j;

  for (i = 0; i < LENGTH; ) {
    for (j = 0; j < LENGTH; ) {
      W[i][j] = ((DATA_TYPE)i * j + 1) / LENGTH;
      j++;
    }
    i++;
  }
}

/* Define the live-out variables. Code is not executed unless
 POLYBENCH_DUMP_ARRAYS is defined. */
static inline
void print_array(int argc, char** argv) {
  int i, j;
#ifndef POLYBENCH_DUMP_ARRAYS
  if(argc > 42 && !strcmp(argv[0], ""))
#endif
  {
    fprintf(stderr, DATA_PRINTF_MODIFIER, out);
    fprintf(stderr, "\n");
  }
}

#pragma hmpp myCodelet codelet, target=CUDA
void codelet(int length, int tsteps,
             DATA_TYPE out[1],
             DATA_TYPE sum_c[LENGTH][LENGTH][LENGTH],
             DATA_TYPE c[LENGTH][LENGTH],
             DATA_TYPE W[LENGTH][LENGTH]) {
  int iter, i, j, k;
 *out = 0;
  for (iter = 0; iter < tsteps; iter++) {
    for (i = 0; i <= length - 1; i++)
      for (j = 0; j <= length - 1; j++)
        c[i][j] = 0;

    for (i = 0; i <= length - 2; i++) {
      for (j = i + 1; j <= length - 1; j++) {
        sum_c[i][j][i] = 0;
        for (k = i + 1; k <= j - 1; k++)
          sum_c[i][j][k] = sum_c[i][j][k - 1] + c[i][k] + c[k][j];
        c[i][j] = sum_c[i][j][j - 1] + W[i][j];
      }
    }
    *out += c[0][length - 1];
  }

}
int main(int argc, char** argv) {
  int iter, i, j, k;
  int length = LENGTH;
  int tsteps = TSTEPS;

  /* Initialize array. */
  init_array();

  /* Start timer. */
  timer_start();

  /* Cheat the compiler to limit the scope of optimisation */
  if(argv[0]==0) {
    init_array();
  }


#pragma hmpp myCodelet callsite
  codelet(length,tsteps,&out, sum_c, c, W);

  /* Cheat the compiler to limit the scope of optimisation */
  if(argv[0]==0) {
    print_array(argc, argv);
  }

  /* Stop and print timer. */
  timer_stop_display(); ;

  print_array(argc, argv);

  return 0;
}
