//
//  Copyright 2017 Apple. All rights reserved.
//

#include "AtomicFile.h"
#include <err.h>

#if 0
static void
fill_disk(const char *path)
{
    int fd = ::open(path, O_CREAT|O_RDWR, 0600);
    if (fd < 0)
        errx(1, "failed to create fill file");

    uint8 buffer[1024] = {};
    ::memset(reinterpret_cast<void *>(buffer), 0x77, sizeof(buffer));

    for (unsigned count = 0; count < 1000; count++) {
        if (::write(fd, buffer, sizeof(buffer)) != sizeof(buffer)) {
            warn("write fill file failed");
            break;
        }
    }
    if (close(fd) < 0)
        warn("close fill file failed");
}
#endif

int
main(int argc, char **argv)
{
    int fail = 0;

    if (argc != 2)
        errx(1, "argc != 2");

    try {
        AtomicFile file(argv[1]);

        RefPointer<AtomicTempFile> temp = file.write();

        unsigned count = 0;
        uint8 buffer[1024] = {};
        ::memset(reinterpret_cast<void *>(buffer), 0xff, sizeof(buffer));

        for (count = 0; count < 1000; count++) {
            temp->write(AtomicFile::FromEnd, 0, buffer, sizeof(buffer));
        }

        temp->commit();
        temp = NULL;
    } catch (...) {
        fail = 1;
    }
    if (fail)
        errx(1, "failed to create new file");
    return 0;
}



