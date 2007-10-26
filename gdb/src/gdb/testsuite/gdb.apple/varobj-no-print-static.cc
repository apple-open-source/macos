#include <iostream>

class Foo
{
public:
  Foo (int inFirst, int inSecond, int inThird, int inFourth)
  {
    first = inFirst;
    second = inSecond;
    third = inThird;
    fourth = inFourth;
  };

  int first;
  static int firstStatic;
protected:
  int second;
  static int secondStatic;
  static int thirdStatic;
  int third;
private:
  static int fourthStatic;
  int fourth;
};

int Foo::firstStatic = 5;
int Foo::secondStatic = 15;
int Foo::thirdStatic = 25;
int Foo::fourthStatic = 35;

int main ()
{
  Foo mine (10, 20, 30, 40);

  std::cout << "Hello World" << mine.first << std::endl;
  
}
