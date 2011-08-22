/* Check the memory access strictness property:no information
 * available about n
 *
 * Issue:
 *
 */

#include <stdio.h>
#define SIZE 10

int sequence07(int n, int m, int x[SIZE], int y[SIZE][SIZE])
{
  int i, j, k;
  for(i=0;i<n;i++) {
    for(j=0;j<m;j++) {
      x[i] = x[i] + y[i][i];
      x[i] = x[i] + y[i][j];
      x[i] = x[i] + y[i][i];
      x[i] = x[i] + y[i][i];
    }
    k = x[i];
  }
  return k;
}

int main(int argc, char **argv)
{
  int i, j, n, m;
  int x[SIZE], y[SIZE][SIZE];

  for (i=0 ; i < SIZE ; i++)
    for (j=0 ; j < SIZE ; j++)
      //scanf("%d", &y[i][j]);
      y[i][j] = 100*i+j;

  scanf("%d", &n);
  scanf("%d", &m);

  i = sequence07(n, m, x, y);

  printf("%d\n", i);
  return 0;
}
