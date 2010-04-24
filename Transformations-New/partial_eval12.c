/* bug seen in Transformations/eval.c: how do we guess effects on
   Fortran parameters? How dowe hanfle C differently from Fortran? */

#include <stdio.h>

int main()
{
  int i;
  i = (3)%(2);
  printf("pos-pos 3%c2=%d (must be 1)\n", '%',i);
  i = (-3)%(2);
  printf("neg pos (-3)%c2=%d (must be -1)\n", '%', i);
  i = (3)%(-2);
  printf("pos neg 3%c(-2)%d (must be 1)\n", '%', i);
  i = (-3)%(-2);
  printf("neg neg (-3)%c(-2)=%d (must be -1)\n", '%', i);
  return i;
}
