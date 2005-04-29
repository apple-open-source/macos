#include <stdio.h>
#include "mk4.h"
#include "mk4str.h"

void QuickTest(int pos_, int len_)
{
  c4_ViewProp p1 ("_B");
  c4_IntProp p2 ("p2");
  
  c4_Storage s1;
  c4_View v1 = s1.GetAs("v1[_B[p2:I]]");

  int n = 0;
  static int sizes[] = {999, 999, 999, 3, 0};

  for (int i = 0; sizes[i]; ++i) {
    c4_View v;
    for (int j = 0; j < sizes[i]; ++j)
      v.Add(p2 [++n]);
    v1.Add(p1 [v]);
  }

  c4_View v2 = v1.Blocked();
  printf("%d\n", v2.GetSize());
    
  v2.RemoveAt(pos_, len_);
  printf("%d\n", v2.GetSize());

  puts("done");
}

int main(int argc, char** argv)
{
  QuickTest(999, 1200);
}
