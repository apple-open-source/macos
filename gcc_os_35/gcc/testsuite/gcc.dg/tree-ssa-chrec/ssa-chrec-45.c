/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */

/* That's a reduced testcase of one of my favourite simulation programs.
   This is also known under the name: "Newton's falling apple".
   The general version is known under the name: "the N-body simulation problem".  
   
   The physics terminology is the best to describe the scalar evolution algorithm:
   - first determine the initial conditions of the system,
   - then analyze its evolution.
*/

double Newton_s_apple ()
{
  /* Initial conditions.  */
  double g = 10.0;
  double speed_z = 0;
  double altitude = 3000;
  double delta_t = 0.1;
  double total_time = 0;
  
  /* Laws of evolution.  */
  while (altitude > 0.0)
    {
      speed_z += g * delta_t;
      altitude -= speed_z * delta_t;
      total_time += delta_t;
    }
  
  return total_time;
}

/*
  speed_z  ->  {0.0, +, 1.0e+0}_1
  altitude  ->  {3.0e+3, +, {(0.0 + 1.0e+0) * 1.00000000000000005551115123125782702118158340454e-1 * -1, +, 1.0e+0 * 1.00000000000000005551115123125782702118158340454e-1 * -1}_1}_1

  When computing evolutions in the "symbolic as long as possible" strategy, 
  the analyzer extracts only the following:
  
  altitude  ->  {3.0e+3, +, T.2_11 * -1}_1
  
*/

/* FIXME. */
