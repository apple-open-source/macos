// On targets that don't support weak symbols, we require an explicit
// instantiation of arr.
// APPLE LOCAL darwin native
// excess errors test - XFAIL *-*-aout *-*-coff *-*-hpux* *-*-hms *-*-darwin*

template<class T>
struct A {
  static T arr[5];
};

template <class T>
T A<T>::arr[5] = { 0, 1, 2, 3, 4 };

int main ()
{
  return A<int>::arr[0];
}
