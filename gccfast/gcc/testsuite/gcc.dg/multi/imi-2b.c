/* { dg-do dummy } */
struct bar
{
  int y;  
};

extern int getx (struct bar *); /* { dg-error "conflicting types" } */
