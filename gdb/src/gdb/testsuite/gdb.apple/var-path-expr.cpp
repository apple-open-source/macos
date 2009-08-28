#include <iostream>

class Container
{
public:
  Container () { errorNo = -1; }
  virtual int report ()
  {
    return errorNo;
  }
private:
  int errorNo;
};

class SomeOtherStuff
{
public:
  SomeOtherStuff (int inInt, double inDouble) : myInt (inInt), myDouble(inDouble) {};
  int cookUpANumber () { return myInt * (int) myDouble; }
private:
  int myInt;
  double myDouble;
};

class IntContainer: public Container, SomeOtherStuff
{
public: 
  IntContainer (int inInt, int otherInt, double otherDouble): intValue(inInt), 
               SomeOtherStuff(otherInt, otherDouble) {};
  virtual int report ()
  {
    return intValue * cookUpANumber ();
  }
private:
  int intValue;
};

class FancyIntContainer: public IntContainer
{
public:
  FancyIntContainer (int inInt, int inMultiplier): IntContainer (inInt, 6, 5.0), 
                    multiplier(inMultiplier) {};
  virtual int report () 
  {
    return multiplier * IntContainer::report ();
  }
private:
  int multiplier;
};

class Foo
{
public:
  Foo (Container *inContainer) {
    contents = inContainer;
  }

  int report (void) 
  {
    return contents->report (); 
  }
private:
  Container *contents;
};

struct inner
{
    int first;
    double second;
    const char *third;
};

struct plainOld
{
    struct inner embedded;
    struct inner *pointer;
    int blubby;
};

int main ()
{
  FancyIntContainer myContainer (5, 10);
  FancyIntContainer *myContPtr = new FancyIntContainer (5, 10);

  Foo myFoo(&myContainer);
  Foo *myFooPtr = new Foo (myContPtr);

  // std::cout << myFoo.report() << " and " << myFooPtr->report () << ".\n";


  struct inner myInner = {5, 6.0, "7.0" };
  struct plainOld plainStruct = { myInner, &myInner, 5 };
  struct plainOld *plainPtr = new struct plainOld;
  plainPtr->embedded = myInner;
  plainPtr->pointer = &myInner;
  plainPtr->blubby = 6;

  /* SET BREAKPOINT ON NEXT LINE */ 
  std::cout << plainStruct.blubby << " and " << plainPtr->blubby << ".\n";
  
  return 0;

}
