/* { dg-do dummy } */
struct 
{
  int x;
} x, *x_1 = &x, *x_2 = &x;

extern void foo(void);

int main(void)
{
  foo();
}
