/* #define YAML_IS_JSON 1 */
#include "perl_common.h"

#define YAML_IS_JSON 1
#include "perl_syck.h"

#undef YAML_IS_JSON
#include "perl_syck.h"

MODULE = YAML::Syck		PACKAGE = YAML::Syck		

PROTOTYPES: DISABLE

SV *
LoadYAML (s)
	char *	s

SV *
DumpYAML (sv)
	SV *	sv


SV *
LoadJSON (s)
	char *	s

SV *
DumpJSON (sv)
	SV *	sv

