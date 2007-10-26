// { dg-do assemble  }
// GROUPS passed templates default-arguments
template <class I>
// APPLE LOCAL mainline 2006-10-13 3904173
class Klasse {
public:
  void func1(int n=1);
  void func2(int d) {}
};
template <class I> 
// APPLE LOCAL mainline 2006-10-13 3904173
void Klasse<I>::func1(int n) {}

//if this is replaced by:
// APPLE LOCAL mainline 2006-10-13 3904173
//void Klasse<I>::func1(int n=1) {}
//the code compiles.

int main() {
  // APPLE LOCAL mainline 2006-10-13 3904173
  Klasse<int> C;
  return 0;
}
