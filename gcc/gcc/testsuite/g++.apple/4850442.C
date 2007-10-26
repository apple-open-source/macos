/* APPLE LOCAL file 4850442 */
/* { dg-do compile { target "*-*-darwin*" } } */
/* { dg-options "-m64 -Os -fpermissive" } */
typedef long unsigned int size_t;
static char token[256], *cursor1;
size_t strlen (const char *);
static int get_literal_type (void)
{
  if (cursor1[0]=='/')
    return 11;
}

int bar (void)
{
   switch (get_literal_type ())
   {
     case 10:
       if (token[strlen (token) - 1] == '"')
         break; 
     default:
       return 1;
   }
   return 0;
}
