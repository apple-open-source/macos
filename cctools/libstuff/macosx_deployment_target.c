#include <stdlib.h>
#include <string.h>
#include <stuff/errors.h>
#include <stuff/macosx_deployment_target.h>

struct macosx_deployment_target_pair {
    const char *name;
    enum macosx_deployment_target_value value;
};

static const struct macosx_deployment_target_pair
    macosx_deployment_target_pairs[] = {
    { "10.1", MACOSX_DEPLOYMENT_TARGET_10_1 },
    { "10.2", MACOSX_DEPLOYMENT_TARGET_10_2 },
    { NULL, 0 }
};

/*
 * get_macosx_deployment_target() indirectly sets the value and the name with
 * the specified MACOSX_DEPLOYMENT_TARGET environment variable or the current
 * default if not specified.
 */
__private_extern__
void
get_macosx_deployment_target(
enum macosx_deployment_target_value *value,
const char **name)
{
    unsigned long i;
    char *p;

	/* the current default */
	*value = MACOSX_DEPLOYMENT_TARGET_10_1;
	*name = "10.1";

	/*
	 * Pick up the Mac OS X deployment target environment variable.
	 */
	p = getenv("MACOSX_DEPLOYMENT_TARGET");
	if(p != NULL){
	    for(i = 0; macosx_deployment_target_pairs[i].name != NULL; i++){
		if(strcmp(macosx_deployment_target_pairs[i].name, p) == 0){
		    *value = macosx_deployment_target_pairs[i].value;
		    *name = macosx_deployment_target_pairs[i].name;
		    break;
		}
	    }
	    if(macosx_deployment_target_pairs[i].name == NULL){
		warning("unknown MACOSX_DEPLOYMENT_TARGET environment variable "
			"value: %s ignored (using %s)", p, *name);
	    }
	}
}
