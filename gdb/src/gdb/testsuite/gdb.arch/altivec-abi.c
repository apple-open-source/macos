#ifndef __APPLE__
#include <altivec.h>
#endif

#ifdef __APPLE__
#define VDECL4(type, v1, v2, v3, v4) (type) (v1, v2, v3, v4)
#define VDECL8(type, v1, v2, v3, v4, v5, v6, v7, v8) (type) (v1, v2, v3, v4, v5, v6, v7, v8)
#define VDECL16(type, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16) (type) (v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16)
#else
#define VDECL4(type, v1, v2, v3, v4) ((type) {v1, v2, v3, v4})
#define VDECL8(type, v1, v2, v3, v4, v5, v6, v7, v8) ((type) {v1, v2, v3, v4, v5, v6, v7, v8})
#define VDECL16(type, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16) ((type) {v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16})
#endif

vector short             vshort = VDECL8 (vector signed short, 111, 222, 333, 444, 555, 666, 777, 888);
vector unsigned short   vushort = VDECL8 (vector unsigned short, 100, 200, 300, 400, 500, 600, 700, 800);
vector signed int          vint = VDECL4 (vector signed int, -10, -20, -30, -40);
vector unsigned int       vuint = VDECL4 (vector unsigned int, 1111, 2222, 3333, 4444);
vector signed char        vchar = VDECL16 (vector signed char, 'a','b','c','d','e','f','g','h','i','l','m','n','o','p','q','r');
vector unsigned char     vuchar = VDECL16 (vector unsigned char, 'A','B','C','D','E','F','G','H','I','L','M','N','O','P','Q','R');
vector float             vfloat = VDECL4 (vector float, 1.25, 3.75, 5.5, 1.25);

vector signed short      vshort_d = VDECL8 (vector signed short, 0, 0, 0, 0, 0, 0, 0, 0);
vector unsigned short   vushort_d = VDECL8 (vector unsigned short, 0, 0, 0, 0, 0, 0, 0, 0);
vector signed int          vint_d = VDECL4 (vector signed int, 0, 0, 0, 0);
vector unsigned int       vuint_d = VDECL4 (vector unsigned int, 0, 0, 0, 0);
vector signed char        vchar_d = VDECL16 (vector signed char, 'z','z','z','z','z','z','z','z','z','z','z','z','z','z','z','z');
vector unsigned char     vuchar_d = VDECL16 (vector unsigned char, 'Z','Z','Z','Z','Z','Z','Z','Z','Z','Z','Z','Z','Z','Z','Z','Z');
vector float             vfloat_d = VDECL4 (vector float, 1.0, 1.0, 1.0, 1.0);
    
struct test_vec_struct
{
   vector signed short vshort1;
   vector signed short vshort2;
   vector signed short vshort3;
   vector signed short vshort4;
};

static vector signed short test4[4] =
{
   VDECL8 (vector signed short, 1, 2, 3, 4, 5, 6, 7, 8),
   VDECL8 (vector signed short, 11, 12, 13, 14, 15, 16, 17, 18),
   VDECL8 (vector signed short, 21, 22, 23, 24, 25, 26, 27, 28),
   VDECL8 (vector signed short, 31, 32, 33, 34, 35, 36, 37, 38)
};

void
struct_of_vector_func (struct test_vec_struct vector_struct)
{
  vector_struct.vshort1 = vec_add (vector_struct.vshort1, vector_struct.vshort2);
  vector_struct.vshort3 = vec_add (vector_struct.vshort3, vector_struct.vshort4);
}

void
array_of_vector_func (vector signed short *matrix)
{
   matrix[0]  = vec_add (matrix[0], matrix[1]);
   matrix[2]  = vec_add (matrix[2], matrix[3]);
}
 
vector signed int
vec_func (vector short vshort_f,             /* goes in v2 */
          vector unsigned short vushort_f,   /* goes in v3 */
          vector signed int vint_f,          /* goes in v4 */
          vector unsigned int vuint_f,       /* goes in v5 */
          vector signed char vchar_f,        /* goes in v6 */
          vector unsigned char vuchar_f,     /* goes in v7 */
          vector float vfloat_f,             /* goes in v8 */
          vector signed short x_f,           /* goes in v9 */
          vector signed int y_f,             /* goes in v10 */
          vector signed char a_f,            /* goes in v11 */
          vector float b_f,                  /* goes in v12 */
          vector float c_f,                  /* goes in v13 */
          vector signed int intv_on_stack_f)
{

   vector signed int vint_res;
   vector unsigned int vuint_res;
   vector signed short vshort_res;
   vector unsigned short vushort_res;
   vector signed char vchar_res;
   vector float vfloat_res;
   vector unsigned char vuchar_res;

   vint_res  = vec_add (vint_f, intv_on_stack_f);
   vint_res  = vec_add (vint_f, y_f);
   vuint_res  = vec_add (vuint_f, VDECL4 (vector unsigned int, 5, 6, 7, 8));
   vshort_res  = vec_add (vshort_f, x_f);
   vushort_res  = vec_add (vushort_f,
                           VDECL8 (vector unsigned short, 1, 2, 3, 4, 5, 6, 7, 8));
   vchar_res  = vec_add (vchar_f, a_f);
   vfloat_res  = vec_add (vfloat_f, b_f);
   vfloat_res  = vec_add (c_f, VDECL4 (vector float, 1.1, 1.1, 1.1, 1.1));
   vuchar_res  = vec_add (vuchar_f,
			  VDECL16 (vector unsigned char, 'a','a','a','a','a','a','a','a','a','a','a','a','a','a','a','a'));

    return vint_res;
}

void marker(void) {};

int
main (void)
{ 
  vector signed int result = VDECL4 (vector signed int, -1, -1, -1, -1);
  vector signed short x = VDECL8 (vector signed short, 1, 2, 3,4, 5, 6, 7, 8);
  vector signed int y = VDECL4 (vector signed int, 12, 22, 32, 42);
  vector signed int intv_on_stack = VDECL4 (vector signed int, 12, 34, 56, 78);
  vector signed char a = VDECL16 (vector signed char, 'v','e','c','t','o','r',' ','o','f',' ','c','h','a','r','s','.' );
  vector float b = VDECL4 (vector float, 5.5, 4.5, 3.75, 2.25);
  vector float c = VDECL4 (vector float, 1.25, 3.5, 5.5, 7.75);

  vector signed short x_d = VDECL8 (vector signed short, 0, 0, 0, 0, 0, 0, 0, 0);
  vector signed int y_d = VDECL4 (vector signed int, 0, 0, 0, 0);
  vector signed int intv_on_stack_d = VDECL4 (vector signed int, 0, 0, 0, 0);
  vector signed char a_d = VDECL16 (vector signed char, 'q','q','q','q','q','q','q','q','q','q','q','q','q','q','q','q');
  vector float b_d = VDECL4 (vector float, 5.0, 5.0, 5.0, 5.0);
  vector float c_d = VDECL4 (vector float, 3.0, 3.0, 3.0, 3.0);
  
  int var_int = 44;
  short var_short = 3;
  struct test_vec_struct vect_struct;
  
  vect_struct.vshort1 = VDECL8 (vector signed short, 1, 2, 3, 4, 5, 6, 7, 8);
  vect_struct.vshort2 = VDECL8 (vector signed short, 11, 12, 13, 14, 15, 16, 17, 18);
  vect_struct.vshort3 = VDECL8 (vector signed short, 21, 22, 23, 24, 25, 26, 27, 28);
  vect_struct.vshort4 = VDECL8 (vector signed short, 31, 32, 33, 34, 35, 36, 37, 38);

  marker ();

#if 0
  /* This line is useful for cutting and pasting from the gdb command line.  */
  vec_func (vshort,vushort,vint,vuint,vchar,vuchar,vfloat,x,y,a,b,c,intv_on_stack)
#endif

  result = vec_func (vshort,    /* goes in v2 */
                     vushort,   /* goes in v3 */
                     vint,      /* goes in v4 */
                     vuint,     /* goes in v5 */
                     vchar,     /* goes in v6 */
                     vuchar,    /* goes in v7 */
                     vfloat,    /* goes in v8 */
                     x,    /* goes in v9 */
                     y,    /* goes in v10 */
                     a,    /* goes in v11 */
                     b,    /* goes in v12 */
                     c,    /* goes in v13 */
                     intv_on_stack);

   struct_of_vector_func (vect_struct);
   array_of_vector_func (test4);

  return 0;
}

