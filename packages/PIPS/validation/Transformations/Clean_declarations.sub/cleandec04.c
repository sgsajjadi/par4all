#include <stdio.h>
void c_clean_declarations04(int x)
{
  // Hide the formal parameter
  // this is illegal C!?
  int x;
  int y=0;

  if(y) {
    /* This third x is useless, but not its initialization */
    int x = y++;
    int z;
  }
  else {
    int z = y--;
  }
  fprintf(stderr, "x=%dn", x);
} 
