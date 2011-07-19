#include <stdio.h>
#include <fstab.h>

int
main(void)
{
	struct fstab *fsent;

	setfsent();
	while ((fsent = getfsent()) != NULL) {
		printf("%s %s %s %s %s %d %d\n", fsent->fs_spec, fsent->fs_file,
		    fsent->fs_vfstype, fsent->fs_mntops, fsent->fs_type,
		    fsent->fs_freq, fsent->fs_passno);
	}
	return 0;
}
