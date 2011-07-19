
ifnamer: ifnamer.c
	cc -Wall -g -DMAIN               -o ifnamer  ifnamer.c -framework CoreFoundation -framework SystemConfiguration -framework IOKit

snapshot: ifnamer.c
	cc -Wall -g -DTEST_SNAPSHOT      -o snapshot ifnamer.c -framework CoreFoundation -framework SystemConfiguration -framework IOKit

uuid: ifnamer.c
	cc -Wall -g -DTEST_PLATFORM_UUID -o uuid     ifnamer.c -framework CoreFoundation -framework SystemConfiguration -framework IOKit

clean:
	rm -rf ifnamer ifnamer.dSYM snapshot snapshot.dSYM uuid uuid.dSYM

