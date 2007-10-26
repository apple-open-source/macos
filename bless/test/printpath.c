#include <libc.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

int main(int argc, char *argv[]) {

  char *path;
  kern_return_t ret;
  io_registry_entry_t entry = 0;
  io_string_t iopath;

  if(argc != 2) {
    fprintf(stderr, "Usage: %s disk1s1\n", getprogname());
    exit(1);
  }

  path = argv[1];

  //  entry = IORegistryEntryFromPath(kIOMasterPortDefault, path);
  entry = IOServiceGetMatchingService(kIOMasterPortDefault,
				      IOBSDNameMatching(kIOMasterPortDefault, 0, path));

  printf("entry is %p\n", entry);

  if(entry == 0) exit(1);


  ret = IORegistryEntryGetPath(entry, kIOServicePlane, iopath);
  if(ret) {
    fprintf(stderr, "Could not get entry path\n");
    exit(1);
  }
  printf("%s path: %s\n", kIOServicePlane, iopath);

  ret = IORegistryEntryGetPath(entry, kIODeviceTreePlane, iopath);
  if(ret) {
    fprintf(stderr, "Could not get entry path\n");
    exit(1);
  }
  printf("%s path: %s\n", kIODeviceTreePlane, iopath);


 IOObjectRelease(entry); 

  return 0;
}
