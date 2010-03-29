#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    int fd;
    size_t length;
    void *buffer;

    fd = shm_open("FOOBAR", O_RDWR, 0);

    if(-1 == fd) {
	perror("shm_open");
	return EXIT_FAILURE;
    }


    length = sysconf(_SC_PAGESIZE) * 2;

    buffer = mmap(NULL, length,
		  PROT_READ | PROT_WRITE,
		  MAP_FILE | MAP_SHARED, fd, 0);
    
    if(MAP_FAILED == buffer) {
	perror("mmap");
	shm_unlink("FOOBAR");
	close(fd);
	return EXIT_FAILURE;
    }
    
    while(1) {
	unsigned char *cp, *cplimit;

	cp = buffer;
	cplimit = cp + length;

	while(cp < cplimit) {
	    printf("cp %x\n", *cp);
	    ++cp;  
	}

	if(-1 == munmap(buffer, length))
	    perror("munmap");
	
	if(-1 == ftruncate(fd, 10))
	    perror("ftruncate");
    }


    return EXIT_SUCCESS;
}
