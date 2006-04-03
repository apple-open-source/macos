/* APPLE LOCAL file 4258406 */
/* Nested functions are not supported on darwin.  */

/* { dg-compile } */
void foo(int i, int j)
{
	void bar (int k)
	  {	 /* { dg-error "nested functions" } */
		k = j;
	}
}
