#include <stdlib.h>
#include <stdio.h>

#define N 5
#define M 3



void foo(int *p)
{
  *p = 4;
  return;
}



int main() 
{
  int *x;
  int tab[10];
  int *tab2[10];
  int tab3[10][10];
  int **tab4;
  int y;
  

  foo(x);
  foo(tab);
  foo(tab2[4]);
  foo(tab3[5]);
  foo(tab4[6]);
  foo(&y);
  foo(&(tab[1]));
  return 1;
}
