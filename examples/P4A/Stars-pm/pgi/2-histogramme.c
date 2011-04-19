#include <string.h>
#include <math.h>

#include "stars-pm.h"



static void _histogram(int data[NP][NP][NP],
              int histo[NP][NP][NP]) {
  int i,j,k;
  memset(histo, 0, NPART * sizeof(int));
#pragma acc region
  for (i = 0; i < NPART; i++) {
    ++(((int *)histo)[((int*)data)[i]]);
  }
}

void histogram(int data[NP][NP][NP],
               int histo[NP][NP][NP]) {

  TIMING(_histogram(data,histo));
}

