/* Lifted from various headers so we're self-contained. */
#include <ApplicationServices/ApplicationServices.h>

#if 0
typedef int CGError;
typedef int CGWindowLevel;

struct CGPoint {
  float x;
  float y;
};
typedef struct CGPoint CGPoint;

/* Sizes. */

struct CGSize {
  float width;
  float height;
};
typedef struct CGSize CGSize;

/* Rectangles. */

struct CGRect {
  CGPoint origin;
  CGSize size;
};
typedef struct CGRect CGRect;
#endif

typedef CGError       CGSError;
typedef long          CGSWindowCount;
typedef void *        CGSConnectionID;
typedef int           CGSWindowID;
typedef CGSWindowID*  CGSWindowIDList;
typedef CGWindowLevel CGSWindowLevel;
typedef void *CGSRegionObj;
typedef unsigned long CGSDepth;
typedef int32_t                        CGSInt32;
typedef CGSInt32 CGSRowBytes;


extern CGSConnectionID _CGSDefaultConnection ();

extern CGSError CGSGetOnScreenWindowList (CGSConnectionID cid,
                                          CGSConnectionID owner,
                                          CGSWindowCount listCapacity,
                                          CGSWindowIDList list,
                                          CGSWindowCount *listCount);
extern CGSError CGSNewWindow (CGSConnectionID, int, float, float, CGSRegionObj, CGSWindowID *);

/* A expedient until we use windowcount.  */
#define MAXWINS 99

CGSWindowID buf[MAXWINS];
CGSWindowCount num;
CGSWindowID buf2[MAXWINS];
CGSWindowCount num2;

struct win_state {
  CGSWindowID winid;
  CGRect bounds;
  char *contents0;
  char *contents1;
};

struct cg_state {
  int cpid;
  CGSConnectionID conn;
  int numwins;
  struct win_state **wins;
};

/* Since this exists in the inferior's space, we only need the state
   corresponding to the inferior's current state.  */

struct cg_state *cgstate;

/* Set this to 1 to get interesting debugging output to stdout.  */

int debug_cpcg = 1;

int
_gdbcp_cg_save (int cpid)
{
  CGSConnectionID conn;
  CGRect rect;
  CGSDepth depth;
  CGSRowBytes bytesPerRow[2];
  int displayID;
  void *data[2];
  int i, numbytes;
  CGSWindowID buf[MAXWINS];
  struct win_state *winstate;

  conn = _CGSDefaultConnection ();
  if (conn == 0)
    return 0;

  /* Note that a window-less app may still create a cgstate; we don't
     know whether or not it intends to create windows in the future,
     so be ready.  */

  if (cgstate == NULL)
    cgstate = malloc (sizeof (struct cg_state));
  cgstate->cpid = cpid;
  cgstate->conn = conn;
  CGSGetOnScreenWindowList (conn, conn, MAXWINS, buf, &num);
  cgstate->numwins = num;
  cgstate->wins = malloc (num * sizeof (struct win_state *));
  for (i = 0; i < num; ++i)
    {
      cgstate->wins[i] = malloc(sizeof(struct win_state));
      bzero (cgstate->wins[i], sizeof(struct win_state));
      winstate = cgstate->wins[i];
      winstate->winid = buf[i];
      /* Collect properties */
      CGSGetWindowGeometry(conn, buf[i], &(winstate->bounds), NULL, NULL);
      /* Collect contents */
      CGSLockWindowRectBits (conn, buf[i], NULL, &rect,
			     &displayID, &depth, data, bytesPerRow);
      if (debug_cpcg)
	printf ("    %x %x size %f x %f depth %d rb %d %d\n",
		data[0], data[1], rect.size.width, rect.size.height, depth,
		bytesPerRow[0], bytesPerRow[1]);
      /* copy somewhere */
      if (data[0])
	{
	  numbytes = ((int) rect.size.height) * bytesPerRow[0];
	  winstate->contents0 = malloc (numbytes);
	  if (winstate->contents0)
	    memcpy (winstate->contents0, data[0], numbytes);
	  else
	    printf ("warning: contents copy failed due to null pointer\n");
	}
      if (data[1])
	{
	  numbytes = ((int) rect.size.height) * bytesPerRow[1];
	  winstate->contents1 = malloc (numbytes);
	  if (winstate->contents1)
	    memcpy (winstate->contents1, data[1], numbytes);
	  else
	    printf ("warning: contents copy failed due to null pointer\n");
	}
      CGSUnlockWindowRectBits (conn, buf[i], NULL);
    }
  if (debug_cpcg)
    printf("cpid=%d %x %d\n", cgstate->cpid, cgstate->conn, cgstate->numwins);
  for (i = 0; i < num; ++i)
    {
      winstate = cgstate->wins[i];
      if (debug_cpcg)
	printf("  %x (%f,%f @ %f,%f)\n", buf[i],
	       cgstate->wins[i]->bounds.size.width, cgstate->wins[i]->bounds.size.height,
	       cgstate->wins[i]->bounds.origin.x, cgstate->wins[i]->bounds.origin.y
	       );
    }
  return 0;
}

int
_gdbcp_cg_rollback (int cpid)
{
  CGSConnectionID conn;
  CGSWindowID winid;
  int i, j, found;
  struct win_state *oldwinstate;

  conn = _CGSDefaultConnection ();
  if (conn == 0)
    return 0;

  if (cgstate == NULL)
    {
      printf ("warning: no saved cgstate? doing nothing\n");
      return -1;
    }

  if (cgstate->cpid != cpid)
    {
      printf ("warning: cgstate->cpid == %d != cpid == %d?\n",
	      cgstate->cpid, cpid);
    }

  /* Get current state.  */
  conn = _CGSDefaultConnection ();
  CGSGetOnScreenWindowList (conn, conn, MAXWINS, buf2, &num2);
  if (debug_cpcg)
    printf("%x %d\n", conn, num2);
  for (j = 0; j < num2; ++j)
    {
      if (debug_cpcg)
	printf("  %x\n", buf2[j]);
    }
  for (i = 0; i < cgstate->numwins; ++i)
    {
      CGRect bounds;
      CGPoint pnt;
      CGSRegionObj shape;
      int numbytes;

      oldwinstate = cgstate->wins[i];
      /* See if the old saved window still exists.  */
      found = 0;
      for (j = 0; j < num2; ++j)
	{
	  if (oldwinstate->winid == buf2[j])
	    {
	      /* (found, set properties to match old state) */
	      /*
		CGSNewRegionWithRect(&(oldbounds[i]), &shape);
		CGSSetWindowShape (conn, buf2[j], 100.0, 100.0, shape);
	      */
	      pnt.x = oldwinstate->bounds.origin.x; pnt.y = oldwinstate->bounds.origin.y;
	      CGSMoveWindow (conn, buf2[j], &pnt);
	      {
		CGRect rect;
		CGSDepth depth;
		CGSRowBytes bytesPerRow[2];
		int displayID;
		void *data[2];
		/* Get pointer to contents */
		CGSLockWindowRectBits (conn, oldwinstate->winid, NULL, &rect,
				       &displayID, &depth, data, bytesPerRow);
		if (debug_cpcg)
		  printf ("    %x %x size %f x %f depth %d rb %d %d\n",
			  data[0], data[1], rect.size.width, rect.size.height,
			  depth, bytesPerRow[0], bytesPerRow[1]);
		/* copy somewhere */
		if (data[0])
		  {
		    numbytes = ((int) rect.size.height) * bytesPerRow[0];
#if 0 /* uncomment to add colorful stripes */
		    if (oldwinstate->contents0) { int k;
		    for (k = 0; k < 100 * bytesPerRow[0]; ++k)
		      if ((k / bytesPerRow[0]) % 2 == 0)
			((int *) oldwinstate->contents0)[k] = k;
		    }
#endif
		    if (oldwinstate->contents0)
		      memcpy (data[0], oldwinstate->contents0, numbytes);
		    else
		      printf ("warning: contents copy failed due to null pointer\n");
		  }
		if (data[1])
		  {
		    numbytes = ((int) rect.size.height) * bytesPerRow[1];
#if 0 /* uncomment to add colorful stripes */
		    if (oldwinstate->contents1) { int k;
		    for (k = 0; k < 100 * bytesPerRow[0]; ++k)
		      if ((k / bytesPerRow[0]) % 2 == 0)
			((int *) oldwinstate->contents1)[k] = k;
		    }
#endif
		    if (oldwinstate->contents1)
		      memcpy (data[1], oldwinstate->contents1, numbytes);
		    else
		      printf ("warning: contents copy failed due to null pointer\n");
		  }
		CGSUnlockWindowRectBits (conn, oldwinstate->winid, NULL);
	      }
	      oldwinstate->winid = buf2[j] = 0;
	      found = 1;
	      break;
	    }
	}
      /* If the old window is not found among the list of current
	 windows, re-create it from scratch.  */
      if (!found)
	{
	  CGSNewRegionWithRect(&(oldwinstate->bounds), &shape);
	  CGSNewWindow(conn, 2 /* kCGSBufferedBackingType */, 
		       oldwinstate->bounds.origin.x, oldwinstate->bounds.origin.y,
		       shape, &winid);
	  CGSReleaseRegion(shape);
	}
    }
  /* Now go through the remaining current windows and delete any
     that didn't exist in the past.  */
  for (j = 0; j < num2; ++j)
    {
      if (buf2[j])
	{
	  CGSReleaseWindow(conn, buf2[j]);
	  buf2[j] = 0;
	}
    }
  /* Re-get the list of windows.  */
  CGSGetOnScreenWindowList (conn, conn, MAXWINS, buf2, &num2);
  if (debug_cpcg)
    printf("%x %d\n", conn, num2);
  /* Flush everything explicitly. */
  for (j = 0; j < num2; ++j)
    {
      CGRect bounds;
      CGSRegionObj shape;

      if (debug_cpcg)
	printf("  %x\n", buf2[j]);
      CGSGetWindowGeometry(conn, buf2[j], &bounds, NULL, NULL);
      CGSNewRegionWithRect(&bounds, &shape);
      CGSFlushWindow (conn, buf2[j], shape);
      CGSReleaseRegion(shape);
    }
  return 0;
}

int
_gdbcp_cg_delete (int cpid)
{
  return 0;
}
