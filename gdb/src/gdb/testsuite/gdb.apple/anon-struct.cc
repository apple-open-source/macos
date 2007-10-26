#include <iostream>

struct foo
{
public:
  int a;
  union
  {
    int b;
    double c;
  };
  struct
  {
    int d;
    double e; 
  };
private:
  int f;
  union 
  {
    int g;
    double h;
  };
  struct
  {
    int i;
    double j;
  };
public:
  foo (int in_a, int in_b, int in_d, double in_e, int in_f, int in_g, int in_i, double in_j) :
    a(in_a), b(in_b), d(in_d), e(in_e), f(in_f), g(in_g), i(in_i), j(in_j) {;};
};

int main ()
{
  struct foo mine (1, 2, 3, 4.0, 5, 6, 7, 8.0);
  struct foo *nother = &mine;

  std::cout << "Hello World" << mine.a << mine.d << std::endl;
  
}
