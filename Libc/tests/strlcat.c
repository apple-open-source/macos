#include <bsdtests.h>
#include <mach/mach_types.h>
#include <sys/mman.h>
#include <string.h>

static const char* qbf = "The quick brown fox jumps over the lazy dog";
static const char* lynx = "Lynx c.q. vos prikt bh: dag zwemjuf!";

int
main(void)
{
	test_start("strlcat");

	void *ptr = mmap(NULL, PAGE_SIZE*2, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
	test_ptr_not("mmap", ptr, MAP_FAILED);

	test_errno("mprotect", mprotect(ptr+PAGE_SIZE, PAGE_SIZE, PROT_READ), 0);

	off_t offset = strlen(qbf)+strlen(lynx)+1;
	char *dst = (ptr+PAGE_SIZE)-offset;
	strcpy(dst, qbf);

	size_t res = strlcat(dst, lynx, offset);
	test_long("strlcat", res, offset-1);
	test_long("memcmp", memcmp(dst, qbf, strlen(qbf)), 0);
	test_long("memcmp", memcmp(dst+strlen(qbf), lynx, strlen(lynx)), 0);
	test_long("null-term", dst[offset], 0);

	memset(ptr, '\0', PAGE_SIZE);

	offset = strlen(qbf)+(strlen(lynx)/2)+1;
	dst = (ptr+PAGE_SIZE)-offset;
	strcpy(dst, qbf);

	res = strlcat(dst, lynx, offset);
	test_long("strlcat", res, strlen(qbf)+strlen(lynx));
	test_long("memcmp", memcmp(dst, qbf, strlen(qbf)), 0);
	test_long("memcmp", memcmp(dst+strlen(qbf), lynx, offset-strlen(qbf)-1), 0);
	test_long("overrun", *(char*)(ptr+PAGE_SIZE), 0);
	test_long("null-term", dst[offset], 0);

	memset(ptr, '\0', PAGE_SIZE);

	offset = strlen(qbf)-4;
	dst = (ptr+PAGE_SIZE)-offset;
	strncpy(dst, qbf, offset);

	res = strlcat(dst, lynx, offset);
	test_long("strlcat", res, offset+strlen(lynx));
	test_long("memcmp", memcmp(dst, qbf, offset), 0);
	test_long("overrun", *(char*)(ptr+PAGE_SIZE), 0);
	test_long("null-term", dst[offset], 0);

	test_stop();
	return EXIT_SUCCESS;
}