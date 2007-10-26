#include "cpp_variable.h"

static void do_simple_class_tests (void);

int
VB::fvb_pub () {return 300 + vb_pub_int;}

int
VB::vvb_pub () {return 400 + vb_pub_int;}

int
V::f () {return 600 + v_pub_int;}

int
V::vv () {return 400 + v_pub_int;}

int
VC::fvc () {return 300 + vc_pub_int;}

int
VC::vfvc () {return 100 + vc_pub_int;}

main ()
{
  do_simple_class_tests ();
}

static void
do_simple_class_tests (void)
{
  V *v = new V;
  V vv;
}
