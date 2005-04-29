// APPLE LOCAL file May 30 2003
// Radar 3035898: implicit instantiation of static array in class template.
// Origin: Matt Austern <austern@apple.com>
// {dg-do link }

template <class T>
struct X
{
  static int array[1];
};

template <class T> int X<T>::array[1] = {0};

int main() {
  return   X<double>::array[0];
}
