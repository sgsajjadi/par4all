#include <stdio.h>
void lonesome_cowboy()
{
    int i,j,n=12;
    int a[n][n];
    for(i=0;i<n;i++)
        for(j=0;j<n;j++)
            a[i][j]=0;
    for(i=1;i<n-1;i++)
isolate:
        for(j=1;j<n;j++)
        {
            if(j<10&&j>=2) a[i][j]=1;
        }
    /* compute trace */
    for(i=0,j=0;i<n;i++)
        j+=a[i][i];
    printf("%d",j);
}
int main(int argv, char *argc[])
{
    lonesome_cowboy();
    return 0;
}
