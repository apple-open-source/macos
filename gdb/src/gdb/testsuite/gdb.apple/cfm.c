#include <stdlib.h>
#include <assert.h>

#include <Carbon/Carbon.h>

void f ()
{
	printf("Done loading libraries.\n");
}

int main (int argc, char *argv[])
{
  unsigned int i;
  unsigned int ncarbon = 64;

  for (i = 1; i <= ncarbon; i++) {

    FSRef ref;
    FSSpec spec;
    Boolean isdir;
    char buf[1024];
    OSStatus ret = 0;
    CFragConnectionID id;
    Ptr addr;
    Str255 err;
    Str255 path;

    sprintf (buf, "/tmp/cfm/%d.cfm", i);

    ret = FSPathMakeRef ((const UInt8 *)buf, &ref, &isdir);
    assert (ret == noErr); 

    ret = FSGetCatalogInfo (&ref, kFSCatInfoNone, NULL, NULL, &spec, NULL);
    assert (ret == noErr); 

    c2pstrcpy (path, buf);
    ret = GetDiskFragment (&spec, 0, 0, path, kLoadCFrag, &id, &addr, err);
  }

  f ();

  return (EXIT_SUCCESS);
}
