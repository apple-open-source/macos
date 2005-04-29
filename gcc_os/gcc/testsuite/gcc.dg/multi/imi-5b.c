/* { dg-do dummy } */
extern double bar(int, int); /* { dg-error "conflicting types" } */

int main(void)
{
  bar(1, 2);
}

