/* { dg-do run } */
/* { dg-options "" } */

/* Make sure a large data section does not cause out-of-range
   branch errors. */

#include <stdlib.h>
#include <string.h>

#define SIZE 150000

struct log {
   char y[98];
   char z[25];
};

struct log a[SIZE] = {{"",""}};
struct log b[SIZE]= {{"",""}};
struct log c[SIZE]= {{"",""}};
struct log d[SIZE]= {{"",""}};

int main(int argc, char * argv[])
{
   char *my_title = malloc (strlen("FOO") + 1);
   strcpy (my_title, "FOO");
   return 0;
};
