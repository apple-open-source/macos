typedef struct _NSPoint { float x; float y; } NSPoint;
typedef struct _NSSize { float width; float height; } NSSize;
typedef struct _NSRect { NSPoint origin; NSSize size; } NSRect;

int
main ()
{
  int a = getpid ();
  NSRect b;
  b.origin.x = 10 + a;
  b.origin.y = 5 + a;
  b.size.width = 10 * a;
  b.size.height = 20 * a;
  puts ("completed");  // Finished setting up my NSRect

  return b.origin.x;
}
