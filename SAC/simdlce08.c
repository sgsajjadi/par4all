
/*
 * file for stencil_3d_8.c
 */
/* PIPS include guard begin: #include "stdio.h" */
#include "stdio.h"
/* PIPS include guard end: #include "stdio.h" */

typedef float t_real;

// real, dimension(n1,n2,n3) :: u, v
// real, dimension(-L:L) :: c

//  parameter(L=4, n1=100, n2=100, n3=100)
const int L = 4;
const int n1 = 32;
const int n2 = 16;
const int n3 = 16;

void stencil8(t_real u[n1][n2][n3], t_real v[n1][n2][n3], t_real c[2L+1], int is1, int ie1, int is2, int ie2, int is3, int ie3);

// initialize the array, with the give value
void init(t_real u[n1][n2][n3], t_real val);

// sum all the elements of the array
t_real sum(t_real u[n1][n2][n3]);
typedef float __m128[4];

int main(void);
void stencil8(t_real u[n1][n2][n3], t_real v[n1][n2][n3], t_real c[2L+1], int is1, int ie1, int is2, int ie2, int is3, int ie3)
{
   // Stencil length : 2*L
   
   int i1, i2;
   //PIPS generated variable
   int LU_IND0;
   //PIPS generated variable
   t_real c10, c40, c_10;
   //SAC generated temporary array
   float aligned[4] = {0, 0, 0, 0}, aligned0[4] = {0, 0, 0, 0}, aligned1[4] = {0, 0, 0, 0}, aligned2[4] = {0, 0, 0, 0}, aligned3[4] = {0, 0, 0, 0}, aligned4[4] = {0, 0, 0, 0}, aligned5[4] = {0, 0, 0, 0}, aligned6[4] = {0, 0, 0, 0}, aligned7[4] = {0, 0, 0, 0};
   //SAC generated temporary array
   int aligned17[4] = {3, 3, 3, 3};
   //SAC generated temporary array
   float aligned19[4] = {0, 0, 0, 0}, aligned20[4] = {0, 0, 0, 0}, aligned21[4] = {0, 0, 0, 0}, aligned22[4] = {0, 0, 0, 0}, aligned23[4] = {0, 0, 0, 0}, aligned24[4] = {0, 0, 0, 0}, aligned25[4] = {0, 0, 0, 0}, aligned26[4] = {0, 0, 0, 0}, aligned27[4] = {0, 0, 0, 0}, aligned28[4] = {0, 0, 0, 0};
   //PIPS generated variable
   __m128 vec0_0, vec3_0, vec6_0, vec9_0, vec12_0, vec15_0, vec18_0, vec21_0, vec24_0, vec25_0, vec27_0, vec28_0, vec30_0, vec31_0, vec33_0, vec34_0, vec36_0, vec37_0, vec39_0, vec40_0, vec42_0, vec43_0, vec45_0, vec46_0, vec48_0, vec49_0, vec51_0, vec52_0, vec54_0, vec55_0, vec57_0, vec58_0, vec60_0, vec61_0, vec63_0, vec64_0, vec66_0, vec67_0, vec69_0, vec70_0, vec72_0, vec75_0, vec76_0, vec78_0, vec79_0, vec80_0, vec81_0, vec83_0, vec84_0, vec86_0, vec87_0, vec90_0, vec92_0, vec93_0, vec95_0, vec96_0, vec98_0, vec101_0;

   aligned19[0] = c[0];
   aligned19[1] = c[1];
   aligned19[3] = c[2];
   aligned23[2] = c[3];
   aligned7[0] = c[4];
   c40 = c[8];
   aligned23[3] = c[7];
   aligned23[0] = c[6];
   aligned23[1] = c[5];
   
   //do i1=is1+L,ie1-L
   
   //do i1=is1+L,ie1-L
   for(i1 = 4; i1 <= 27; i1 += 1)
      //do i2=is2+L,ie2-L
      //do i2=is2+L,ie2-L
      for(i2 = 4; i2 <= 11; i2 += 1) {
         //  do i3=is3+L,ie3-L
         //PIPS:SAC generated float vector(s)
         SIMD_LOAD_V4SF(vec25_0, &aligned7[0]);
         SIMD_LOAD_GENERIC_V4SF(vec28_0, aligned[0], aligned0[0], aligned[1], aligned1[0]);
         SIMD_LOAD_GENERIC_V4SF(vec31_0, aligned0[1], aligned[2], aligned2[0], aligned1[1]);
         SIMD_LOAD_GENERIC_V4SF(vec34_0, aligned0[2], aligned[3], aligned2[1], aligned1[2]);
         SIMD_LOAD_GENERIC_V4SF(vec37_0, aligned0[3], aligned3[0], aligned2[2], aligned1[3]);
         SIMD_LOAD_GENERIC_V4SF(vec40_0, aligned4[0], aligned3[1], aligned2[3], aligned5[0]);
         SIMD_LOAD_GENERIC_V4SF(vec43_0, aligned4[1], aligned3[2], aligned6[0], aligned5[1]);
         SIMD_LOAD_GENERIC_V4SF(vec46_0, aligned4[2], aligned3[3], aligned6[1], aligned5[2]);
         SIMD_LOAD_GENERIC_V4SF(vec49_0, aligned4[3], aligned6[2], aligned5[3], aligned6[3]);
         SIMD_LOAD_V4SI_TO_V4SF(vec52_0, &aligned17[0]);
         SIMD_LOAD_V4SF(vec55_0, &aligned19[0]);
         SIMD_LOAD_GENERIC_V4SF(vec58_0, aligned19[1], aligned19[0], c_10, aligned19[3]);
         SIMD_LOAD_GENERIC_V4SF(vec61_0, aligned19[1], c10, c_10, aligned19[3]);
         SIMD_LOAD_V4SF(vec64_0, &aligned23[0]);
         SIMD_LOAD_GENERIC_V4SF(vec67_0, aligned23[0], aligned23[1], c40, aligned23[3]);
         SIMD_LOAD_GENERIC_V4SF(vec70_0, aligned19[1], aligned19[0], aligned23[2], aligned19[3]);
         SIMD_LOAD_GENERIC_V4SF(vec76_0, aligned23[0], c40, aligned23[3], c40);
         SIMD_LOAD_GENERIC_V4SF(vec80_0, aligned20[1], aligned21[0], aligned26[0], aligned22[0]);
         SIMD_LOAD_GENERIC_V4SF(vec79_0, aligned20[0], aligned20[2], aligned21[1], aligned26[1]);
         SIMD_LOAD_GENERIC_V4SF(vec83_0, aligned20[3], aligned21[3], aligned26[3], aligned22[3]);
         SIMD_LOAD_GENERIC_V4SF(vec86_0, aligned21[2], aligned26[2], aligned22[2], aligned24[2]);
         SIMD_LOAD_GENERIC_V4SF(vec92_0, aligned22[1], aligned24[1], aligned25[1], aligned27[1]);
         SIMD_LOAD_GENERIC_V4SF(vec95_0, aligned24[0], aligned25[0], aligned27[0], aligned28[0]);
         SIMD_LOAD_GENERIC_V4SF(vec98_0, aligned24[3], aligned25[3], aligned27[3], aligned28[2]);
         SIMD_LOAD_GENERIC_V4SF(vec101_0, aligned25[2], aligned27[2], aligned28[1], aligned28[3]);
         for(LU_IND0 = 0; LU_IND0 <= 7; LU_IND0 += 4) {
            //PIPS:SAC generated float vector(s)
            __m128 vec1, vec2, vec4, vec5, vec7, vec8, vec10, vec11, vec13, vec14, vec16, vec17, vec19, vec20, vec22, vec23, vec26, vec29, vec32, vec35, vec38, vec41, vec44, vec47, vec50, vec53, vec56, vec59, vec62, vec65, vec68, vec71, vec73, vec74, vec77, vec82, vec85, vec88, vec89, vec91, vec94, vec97, vec99, vec100;
            SIMD_LOAD_V4SF(vec2, &v[i1][i2-4][4+LU_IND0]);
            SIMD_LOAD_V4SF(vec1, &v[i1-4][i2][4+LU_IND0]);
            SIMD_ADDPS(vec0_0, vec1, vec2);
            SIMD_LOAD_V4SF(vec5, &v[i1][i2-3][4+LU_IND0]);
            SIMD_LOAD_V4SF(vec4, &v[i1-3][i2][4+LU_IND0]);
            SIMD_ADDPS(vec3_0, vec4, vec5);
            SIMD_LOAD_V4SF(vec8, &v[i1][i2-2][4+LU_IND0]);
            SIMD_LOAD_V4SF(vec7, &v[i1-2][i2][4+LU_IND0]);
            SIMD_ADDPS(vec6_0, vec7, vec8);
            SIMD_LOAD_V4SF(vec11, &v[i1][i2-1][4+LU_IND0]);
            SIMD_LOAD_V4SF(vec10, &v[i1-1][i2][4+LU_IND0]);
            SIMD_ADDPS(vec9_0, vec10, vec11);
            SIMD_LOAD_V4SF(vec14, &v[i1][1+i2][4+LU_IND0]);
            SIMD_LOAD_V4SF(vec13, &v[1+i1][i2][4+LU_IND0]);
            SIMD_ADDPS(vec12_0, vec13, vec14);
            SIMD_LOAD_V4SF(vec17, &v[i1][2+i2][4+LU_IND0]);
            SIMD_LOAD_V4SF(vec16, &v[2+i1][i2][4+LU_IND0]);
            SIMD_ADDPS(vec15_0, vec16, vec17);
            SIMD_LOAD_V4SF(vec20, &v[i1][3+i2][4+LU_IND0]);
            SIMD_LOAD_V4SF(vec19, &v[3+i1][i2][4+LU_IND0]);
            SIMD_ADDPS(vec18_0, vec19, vec20);
            SIMD_LOAD_V4SF(vec23, &v[i1][4+i2][4+LU_IND0]);
            SIMD_LOAD_V4SF(vec22, &v[4+i1][i2][4+LU_IND0]);
            SIMD_ADDPS(vec21_0, vec22, vec23);
            SIMD_LOAD_V4SF(vec26, &v[i1][i2][4+LU_IND0]);
            SIMD_MULPS(vec24_0, vec25_0, vec26);
            SIMD_LOAD_GENERIC_V4SF(vec29, v[i1][i2][LU_IND0], v[i1][i2][1+LU_IND0], v[i1][i2][1+LU_IND0], v[i1][i2][2+LU_IND0]);
            SIMD_ADDPS(vec27_0, vec28_0, vec29);
            SIMD_LOAD_GENERIC_V4SF(vec32, v[i1][i2][2+LU_IND0], v[i1][i2][2+LU_IND0], v[i1][i2][3+LU_IND0], v[i1][i2][3+LU_IND0]);
            SIMD_ADDPS(vec30_0, vec31_0, vec32);
            SIMD_LOAD_GENERIC_V4SF(vec35, v[i1][i2][3+LU_IND0], v[i1][i2][3+LU_IND0], v[i1][i2][4+LU_IND0], v[i1][i2][4+LU_IND0]);
            SIMD_ADDPS(vec33_0, vec34_0, vec35);
            SIMD_LOAD_GENERIC_V4SF(vec38, v[i1][i2][4+LU_IND0], v[i1][i2][5+LU_IND0], v[i1][i2][5+LU_IND0], v[i1][i2][5+LU_IND0]);
            SIMD_ADDPS(vec36_0, vec37_0, vec38);
            SIMD_LOAD_GENERIC_V4SF(vec41, v[i1][i2][6+LU_IND0], v[i1][i2][6+LU_IND0], v[i1][i2][6+LU_IND0], v[i1][i2][7+LU_IND0]);
            SIMD_ADDPS(vec39_0, vec40_0, vec41);
            SIMD_LOAD_GENERIC_V4SF(vec44, v[i1][i2][7+LU_IND0], v[i1][i2][7+LU_IND0], v[i1][i2][8+LU_IND0], v[i1][i2][8+LU_IND0]);
            SIMD_ADDPS(vec42_0, vec43_0, vec44);
            SIMD_LOAD_GENERIC_V4SF(vec47, v[i1][i2][8+LU_IND0], v[i1][i2][8+LU_IND0], v[i1][i2][9+LU_IND0], v[i1][i2][9+LU_IND0]);
            SIMD_ADDPS(vec45_0, vec46_0, vec47);
            SIMD_LOAD_GENERIC_V4SF(vec50, v[i1][i2][9+LU_IND0], v[i1][i2][10+LU_IND0], v[i1][i2][10+LU_IND0], v[i1][i2][11+LU_IND0]);
            SIMD_ADDPS(vec48_0, vec49_0, vec50);
            SIMD_MULPS(vec51_0, vec52_0, vec24_0);
            SIMD_MULPS(vec54_0, vec55_0, vec27_0);
            SIMD_MULPS(vec57_0, vec58_0, vec30_0);
            SIMD_MULPS(vec60_0, vec61_0, vec36_0);
            SIMD_MULPS(vec63_0, vec64_0, vec39_0);
            SIMD_MULPS(vec66_0, vec67_0, vec42_0);
            SIMD_MULPS(vec69_0, vec70_0, vec33_0);
            SIMD_MULPS(vec72_0, vec67_0, vec45_0);
            SIMD_MULPS(vec75_0, vec76_0, vec48_0);
            SIMD_ADDPS(vec78_0, vec79_0, vec80_0);
            SIMD_ADDPS(vec81_0, vec78_0, vec83_0);
            SIMD_ADDPS(vec84_0, vec81_0, vec86_0);
            SIMD_ADDPS(vec87_0, vec84_0, vec51_0);
            SIMD_ADDPS(vec90_0, vec87_0, vec92_0);
            SIMD_ADDPS(vec93_0, vec90_0, vec95_0);
            SIMD_ADDPS(vec96_0, vec93_0, vec98_0);
            SIMD_ADDPS(vec99, vec96_0, vec101_0);
            SIMD_STORE_V4SF(vec99, &u[i1][i2][4+LU_IND0]);
         }
         SIMD_STORE_V4SF(vec0_0, &aligned[0]);
         SIMD_STORE_V4SF(vec3_0, &aligned0[0]);
         SIMD_STORE_V4SF(vec6_0, &aligned1[0]);
         SIMD_STORE_V4SF(vec9_0, &aligned2[0]);
         SIMD_STORE_V4SF(vec12_0, &aligned3[0]);
         SIMD_STORE_V4SF(vec15_0, &aligned4[0]);
         SIMD_STORE_V4SF(vec18_0, &aligned5[0]);
         SIMD_STORE_V4SF(vec21_0, &aligned6[0]);
         SIMD_STORE_V4SF(vec54_0, &aligned20[0]);
         SIMD_STORE_V4SF(vec57_0, &aligned21[0]);
         SIMD_STORE_V4SF(vec60_0, &aligned22[0]);
         SIMD_STORE_V4SF(vec63_0, &aligned24[0]);
         SIMD_STORE_V4SF(vec66_0, &aligned25[0]);
         SIMD_STORE_V4SF(vec69_0, &aligned26[0]);
         SIMD_STORE_V4SF(vec72_0, &aligned27[0]);
         SIMD_STORE_V4SF(vec75_0, &aligned28[0]);
      }
   ;
}
void init(t_real u[n1][n2][n3], t_real val)
{
   int i = 0, j = 0, k = 0;
   for(i = 0; i <= n1-1; i += 1)
      for(j = 0; j <= n2-1; j += 1)
         for(k = 0; k <= n3-1; k += 1)
            u[i][j][k] = val;
   return;
}
t_real sum(t_real u[n1][n2][n3])
{
   t_real result = 0;
   int i = 0, j = 0, k = 0;
   for(i = 0; i <= n1-1; i += 1)
      for(j = 0; j <= n2-1; j += 1)
         for(k = 0; k <= n3-1; k += 1)
            result += u[i][j][k];
   return result;
}
int main(void)
{

   int is1, ie1, is2, ie2, is3, ie3, i;
   t_real v[n1][n2][n3];
   t_real u[n1][n2][n3];
   t_real c[2*L+1];
   is1 = 0;
   ie1 = n1;
   is2 = 0;
   ie2 = n2;
   is3 = 0;
   ie3 = n3;

   for(i = 0; i <= 2*L+1-1; i += 1)
      c[i] = 3.0;
   
   // Simple case
   init(u, 1.0);
   init(v, 1.0);
   stencil8(u, v, c, is1, ie1, is2, ie2, is3, ie3);

   printf("the sum is : %f\n", sum(u));
   return 0;
}
