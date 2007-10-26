// { dg-do compile }
template<typename T> struct takes_member_ptr;
// APPLE LOCAL mainline 2006-10-13 3904173
template<typename T, typename Klasse> struct takes_member_ptr<T Klasse::*> {};

// APPLE LOCAL begin mainline 2006-10-13 3904173
template<typename T, typename Klasse>
void fun_takes_member_ptr(T Klasse::*) {}
// APPLE LOCAL end mainline 2006-10-13 3904173


template<typename T> struct order_member_ptrs;
// APPLE LOCAL begin mainline 2006-10-13 3904173
template<typename T, typename Klasse> struct order_member_ptrs<T Klasse::*> {};
template<typename R, typename T1, typename Klasse> 
  struct order_member_ptrs<R (Klasse::*)(T1)>
// APPLE LOCAL end mainline 2006-10-13 3904173
  {
    typedef int type;
  };

// APPLE LOCAL begin mainline 2006-10-13 3904173
template<typename R, typename T1, typename Klasse>
  struct order_member_ptrs<R (Klasse::*)(T1) const>
// APPLE LOCAL end mainline 2006-10-13 3904173
  {
    typedef int c_type;
  };

// APPLE LOCAL begin mainline 2006-10-13 3904173
template<typename R, typename T1, typename Klasse>
  struct order_member_ptrs<R (Klasse::*)(T1) volatile>
// APPLE LOCAL end mainline 2006-10-13 3904173
  {
    typedef int v_type;
  };

// APPLE LOCAL begin mainline 2006-10-13 3904173
template<typename R, typename T1, typename Klasse>
  struct order_member_ptrs<R (Klasse::*)(T1) const volatile>
// APPLE LOCAL end mainline 2006-10-13 3904173
  {
    typedef int cv_type;
  };

	  
struct X {
  void bar(float) {}
  void bar_c(float) const {}
  void bar_v(float) volatile {}
  void bar_cv(float) const volatile {}
};

void foo()
{
  sizeof(takes_member_ptr<void (X::*)(float)>);
  sizeof(takes_member_ptr<void (X::*)(float) const>);
  sizeof(takes_member_ptr<void (X::*)(float) volatile>);
  sizeof(takes_member_ptr<void (X::*)(float) const volatile>);
  sizeof(order_member_ptrs<void (X::*)(float)>::type);
  sizeof(order_member_ptrs<void (X::*)(float) const>::c_type);
  sizeof(order_member_ptrs<void (X::*)(float) volatile>::v_type);
  sizeof(order_member_ptrs<void (X::*)(float) const volatile>::cv_type);
  fun_takes_member_ptr(&X::bar);
  fun_takes_member_ptr(&X::bar_c);
  fun_takes_member_ptr(&X::bar_v);
  fun_takes_member_ptr(&X::bar_cv);
}
