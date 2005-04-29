//  Datafile dump utility sample code

#include "mk4.h"
#include "mk4str.h"

#include <stdio.h>

#if defined (unix)
#define try
#define catch(x)  if (0)
#endif     

#if defined (macintosh)
#include /**/ <console.h>
#define d4_InitMain(c,v)  c = ccommand(&v)
#endif

/////////////////////////////////////////////////////////////////////////////
// Recursively display the entire view contents. The results shown do not
// depend on file layout (free space, file positions, flat vs. on-demand).

static void ViewDisplay(const c4_View& v_, int l_ =0)
{
  c4_String types;
  bool hasData = false, hasSubs = false;

    // display header info and collect all data types
  printf("%*s VIEW %5d rows =", l_, "", v_.GetSize());
  for (int n = 0; n < v_.NumProperties(); ++n)
  {
    c4_Property prop = v_.NthProperty(n);
    char t = prop.Type();

    printf(" %s:%c", (const char*) prop.Name(), t);
    
    types += t;
  
    if (t == 'V')
      hasSubs = true;
    else
      hasData = true;
  }
  printf("\n");

  for (int j = 0; j < v_.GetSize(); ++j)
  {
    if (hasData)  // data properties are all shown on the same line
    {
      printf("%*s %4d:", l_, "", j);
      c4_RowRef r = v_[j];
      c4_Bytes data;

      for (int k = 0; k < types.GetLength(); ++k)
      {
        c4_Property p = v_.NthProperty(k);

        switch (types[k])
        {
        case 'I':
          printf(" %ld", (long) ((c4_IntProp&) p) (r));
          break;

#if !q4_TINY
        case 'F':
          printf(" %g", (double) ((c4_FloatProp&) p) (r));
          break;

        case 'D':
          printf(" %.12g", (double) ((c4_DoubleProp&) p) (r));
          break;
#endif

        case 'S':
          printf(" '%s'", (const char*) ((c4_StringProp&) p) (r));
          break;

        case 'M': // backward compatibility
        case 'B':
          (p (r)).GetData(data);
          printf(" (%db)", data.Size());
          break;

        default:
          if (types[k] != 'V')
            printf(" (%c?)", types[k]);
        }
      }

      printf("\n");
    }

    if (hasSubs)  // subviews are then shown, each as a separate block
    {
      for (int k = 0; k < types.GetLength(); ++k)
      {
        if (types[k] == 'V')
        {
          c4_Property prop = v_.NthProperty(k);

          printf("%*s %4d: subview '%s'\n", l_, "", j,
              (const char*) prop.Name());

          c4_ViewProp& vp = (c4_ViewProp&) prop;
          
          ViewDisplay(vp (v_[j]), l_ + 2);
        }
      }
    }
  }
}

/////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv)
{
#ifdef d4_InitMain
  d4_InitMain(argc, argv);
#endif

  const char* msg = 0;
  
  if (argc != 2)
    fprintf(stderr, "Usage: DUMP file\n");
  else
    try
    {
      msg = "could not open data file";

      c4_Storage store (argv[1], false);

      msg = "file may be damaged";

      printf("%s: %d properties\n  %s\n\n",
                  argv[1], store.NumProperties(),
                  (const char*) store.Description());
      ViewDisplay(store);

      msg = 0;
    }
    catch (...)
    {
    }
  
  if (msg)
    fprintf(stderr, "Abnormal termination, %s\n", msg);

  return msg ? 1 : 0;
}

/////////////////////////////////////////////////////////////////////////////
