#include <stdio.h>
#include "cachedsym.h"

typedef struct { int a; char b; } mystruct;

extern void mylibFunc001()
{
        printf("In mylibFunc001()\n");
        mystruct st;
        st.a = 3456;
        st.b = 'a';
}

extern void mylibFunc002()
{
        printf("In mylibFunc002()\n");
}


