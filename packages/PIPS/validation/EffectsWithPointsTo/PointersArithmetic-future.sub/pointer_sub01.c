/* pointer arithmetic with pointers to variable length array type, example from the C norme p84. */
#include<stdio.h>

int main()
{
  int n = 4, m = 3;
  int a[n][m];
  int (*p)[m] = a; // p == &a[0]
  p += 1; // p == &a[1]
  (*p)[2] = 99; // a[1][2] = 99
  n = p-a; // n == 1
  printf("n = %d", n);
  return 0;

}
