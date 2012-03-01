
#include <stdio.h>

void scilab_rt_grayplot_d2i2i2s0d2_(int in00, int in01, double matrixin0[in00][in01], 
    int in10, int in11, int matrixin1[in10][in11], 
    int in20, int in21, int matrixin2[in20][in21], 
    char* scalarin0, 
    int in30, int in31, double matrixin3[in30][in31])
{
  int i;
  int j;

  double val0 = 0;
  int val1 = 0;
  int val2 = 0;
  double val3 = 0;
  for (i = 0; i < in00; ++i) {
    for (j = 0; j < in01; ++j) {
      val0 += matrixin0[i][j];
    }
  }
  printf("%f", val0);

  for (i = 0; i < in10; ++i) {
    for (j = 0; j < in11; ++j) {
      val1 += matrixin1[i][j];
    }
  }
  printf("%d", val1);

  for (i = 0; i < in20; ++i) {
    for (j = 0; j < in21; ++j) {
      val2 += matrixin2[i][j];
    }
  }
  printf("%d", val2);

  printf("%s", scalarin0);

  for (i = 0; i < in30; ++i) {
    for (j = 0; j < in31; ++j) {
      val3 += matrixin3[i][j];
    }
  }
  printf("%f", val3);

}