/* package arithmetic
 *
 * $RCSfile: io.c,v $ (version $Revision$)
 * $Date: 1996/07/13 16:02:32 $, 
 *
 * IO on a Value
 */
#include <stdio.h>
#include <strings.h>

#include "arithmetique.h"

void print_Value(Value v)
{
    (void) printf(VALUE_FMT, v);
}

void fprint_Value(FILE *f, Value v)
{
    (void) fprintf(f, VALUE_FMT, v);
}

char *sprint_Value(char *s, Value v)
{
    return sprintf(s, VALUE_FMT, v);
}

int fscan_Value(FILE *f, Value *pv)
{
    return fscanf(f, VALUE_FMT, pv);
}

int scan_Value(Value *pv)
{
    return scanf(VALUE_FMT, pv);
}

int sscan_Value(char *s, Value *pv)
{
    return sscanf(s, VALUE_FMT, pv);
}

/* this seems a reasonnable upperbound
 */
#define BUFFER_SIZE 50

char * Value_to_string(Value v)
{
    static char buf[BUFFER_SIZE];
    return sprintf(VALUE_FMT "\0", buf, v);
}

/* end of $RCSfile: io.c,v $
 */
