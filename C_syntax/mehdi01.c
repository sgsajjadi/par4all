/* Check that the sequence used as indicing the array is correctly handle */

int func(double B[SIZE][SIZE]) {
 int i=0;
 double B[i++,i-1][0]=0;
} 