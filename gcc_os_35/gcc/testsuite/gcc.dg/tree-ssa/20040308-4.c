/* APPLE LOCAL file lno */
/* { dg-do compile } */
/* { dg-options "-O1 -fdump-tree-phiopt1-details" } */

int t( int i, int k)
{
  int j;
  if(i!=k)
   j = i;
  else
   j = k;
  
  return j;
}

/* We should convert one COND_EXPRs into straightline code.  */
/* { dg-final { scan-tree-dump-times "straightline" 1 "phiopt1"} } */

