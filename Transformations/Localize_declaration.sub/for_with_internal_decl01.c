// _u_col must be localized at the outermost loop level
#include <stdio.h>
int main()
{
  int _u_rows;
  _u_rows = 30000;
  int _u_cols;
  _u_cols = 360;
  double _u_m[1][360];
  /*  prepare result */
  static double _u_zmd[30000][360];
  for (int _u_row=1; _u_row<=_u_rows; _u_row++) {
    for (int _u_col=1; _u_col<=_u_cols; _u_col++) {
      double _tmpxx0 = _u_row * _u_col * 2.0;
      _u_zmd[_u_row-1][_u_col-1] = _tmpxx0;
    }
  }

  for (int _u_row_2=1; _u_row_2<=_u_rows; _u_row_2++) {
    for (int _u_col_2=1; _u_col_2<=_u_cols; _u_col_2++) {
      printf("%lf",_u_zmd[_u_row_2-1][_u_col_2-1]);
    }
  }
  return 0;
}
