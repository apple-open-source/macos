
#define mig_external

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <dirent.h>

/*
 * We don't exit with a non-zero status anywhere here for 2 reasons:
 * - the kernel can continue to create swapfiles in "/private/var/vm/swapfile<index>"
 * - we want this job to run only once at boot and exit regardless of whether:
 *	-- it could clean up the swap directory
 *	-- it could set the prefix for the swapfile name.
 */


static void
clean_swap_directory(const char *path)
{
	DIR *dir;
	struct dirent *entry;
	char buf[1024];

	dir = opendir(path);
	if (dir == NULL) {
		fprintf(stderr,"dynamic_pager: cannot open swap directory %s\n", path);
		exit(0);
	}

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_namlen>= 4 && strncmp(entry->d_name, "swap", 4) == 0) {
			snprintf(buf, sizeof buf, "%s/%s", path, entry->d_name);
			unlink(buf);	
		}
	}

	closedir(dir);
}

int
main(int argc, char **argv)
{
	char default_filename[] = "/private/var/vm/swapfile";
	int ch;
	int err=0;
	static char tmp[1024];
	struct statfs sfs;
	char *q;
	char fileroot[512];

	seteuid(getuid());
	strcpy(fileroot, default_filename);

	while ((ch = getopt(argc, argv, "F:")) != EOF) {
		switch((char)ch) {

		case 'F':
			strncpy(fileroot, optarg, 500);
			break;

		default:
			(void)fprintf(stderr,
			    "usage: dynamic_pager [-F filename]\n");
			exit(0);
		}
	}

	/*
	 * get rid of the filename at the end of the swap file specification
	 * we only want the portion of the pathname that should already exist
	 */
	strcpy(tmp, fileroot);
	if ((q = strrchr(tmp, '/')))
	        *q = 0;

	/*
	 * Remove all files in the swap directory.
	 */
	clean_swap_directory(tmp);

	if (statfs(tmp, &sfs) == -1) {
		/*
	 	 * Setup the swap directory.
	 	 */

       		if (mkdir(tmp, 0755) == -1) {
			(void)fprintf(stderr, "dynamic_pager: cannot create swap directory %s\n", tmp); 
		}
	}

	chown(tmp, 0, 0);

	err = sysctlbyname("vm.swapfileprefix", NULL, 0, fileroot, sizeof(fileroot));
	if (err) {
		(void)fprintf(stderr, "Failed to set swapfile name prefix with error: %d\n", err);
	}

	return (0);
}
