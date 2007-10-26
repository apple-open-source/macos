// PR c++/18962

template<class T1,int N1>
// APPLE LOCAL mainline 2006-10-13 3904173
class Klasse
{
public:
  template <class T2,int N2>
  // APPLE LOCAL mainline 2006-10-13 3904173
  void function( const Klasse<T2,N2>& );
};

template<>
template<class T2,int N2>
// APPLE LOCAL mainline 2006-10-13 3904173
void Klasse<int,1>::function( const Klasse<T2,N2>& param ) 
{
  param; // make sure we use the argument list from the definition.
}

int main()
{
  // APPLE LOCAL begin mainline 2006-10-13 3904173
  Klasse<int,1> instance;
  Klasse<char,2> param;
  // APPLE LOCAL end mainline 2006-10-13 3904173
  instance.function( param );
}
