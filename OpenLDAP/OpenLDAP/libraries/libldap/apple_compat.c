#include <ldap.h>

int ldap_get_lderrno(LDAP *ld, char **m, char **s)
{
	int ld_errno = 0;

	ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &ld_errno);
	return ld_errno;
}

int ldap_version(void *ver)
{
	return LDAP_API_VERSION;
}
