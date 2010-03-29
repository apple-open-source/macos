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
    int b;

    shm_unlink("FOOBAR");

    fd = shm_open("FOOBAR", O_RDWR | O_EXCL | O_CREAT, 
		  S_IRUSR | S_IWUSR | S_IROTH | S_IWOTH);

    if(-1 == fd) {
	perror("shm_open");
	return EXIT_FAILURE;
    }


    length = sysconf(_SC_PAGESIZE) * 2;

    if(-1 == ftruncate(fd, length)) {
	perror("ftruncate");
	shm_unlink("FOOBAR");
	close(fd);
	return EXIT_FAILURE;
    }

    printf("length %zu\n", length);

    buffer = mmap(NULL, length,
		  PROT_READ | PROT_WRITE,
		  MAP_FILE | MAP_SHARED, fd, 0);
    
    if(MAP_FAILED == buffer) {
	perror("mmap");
	shm_unlink("FOOBAR");
	close(fd);
	return EXIT_FAILURE;
    }
    
    b = 0;
    while(1) {
	printf("b %d\n", b);

	memset(buffer, b, length);
	++b;
  
	if(b > 255)
	    b = 0;

	sleep(1);
    }


    return EXIT_SUCCESS;
}
