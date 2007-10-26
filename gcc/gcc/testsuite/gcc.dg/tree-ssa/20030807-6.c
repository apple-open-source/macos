/* { dg-do compile } */
/* { dg-options "-O1 -fdump-tree-dom3" } */
     

/* APPLE LOCAL mainline 4840357 */
void
foo (distance, i, j)
     int distance[13][13];
     int i, j;
{
 if (distance[i][j] < 0)
   distance[i][0] = ((distance[i][j]) < 0 ? -(distance[i][j])  : (distance[i][j]));
}

/* APPLE LOCAL mainline 4840357 */
void
foo2 (distance, i, j)
     int distance[13][13];
     int i, j;
{
 if (distance[i][j] <= 0)
   distance[i][0] = ((distance[i][j]) < 0 ? -(distance[i][j])  : (distance[i][j]));
}

/* APPLE LOCAL mainline 4840357 */
void
foo3 (distance, i, j)
     int distance[13][13];
     int i, j;
{
 if (distance[i][j] > 0)
   distance[i][0] = ((distance[i][j]) < 0 ? -(distance[i][j])  : (distance[i][j]));
}

/* APPLE LOCAL mainline 4840357 */
void
foo4 (distance, i, j)
     double distance[13][13];
     int i, j;
{
 if (distance[i][j] >= 0)
   distance[i][0] = ((distance[i][j]) < 0 ? -(distance[i][j])  : (distance[i][j]));
}

/* There should be no ABS_EXPR.  */
/* { dg-final { scan-tree-dump-times "ABS_EXPR " 0 "dom3"} } */
  

