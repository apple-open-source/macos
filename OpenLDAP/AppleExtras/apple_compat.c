#include <unistd.h>
#include <ldap.h>

int ldap_get_lderrno(LDAP *ld, char **m, char **s)
{
	int rc, lderrno;

	rc = ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &lderrno);
	if (rc != LDAP_SUCCESS)
		return rc;

	if (s != NULL)
	{
		rc = ldap_get_option(ld, LDAP_OPT_ERROR_STRING, s);
		if (rc != LDAP_SUCCESS)
			return rc;
	}

	if (s != NULL)
	{
		rc = ldap_get_option(ld, LDAP_OPT_MATCHED_DN, m);
		if (rc != LDAP_SUCCESS)
			return rc;
	}

	return lderrno;
}

int ldap_set_lderrno(LDAP *ld, int lderrno, const char *m, const char *s)
{
	int rc;

	rc = ldap_set_option(ld, LDAP_OPT_ERROR_NUMBER, &lderrno);
	if (rc != LDAP_SUCCESS)
		return rc;

	if (s != NULL)
	{
		rc = ldap_set_option(ld, LDAP_OPT_ERROR_STRING, s);
		if (rc != LDAP_SUCCESS)
			return rc;
	}

	if (m != NULL)
	{
		rc = ldap_set_option(ld, LDAP_OPT_MATCHED_DN, m);
		if (rc != LDAP_SUCCESS)
			return rc;
	}

	return LDAP_SUCCESS;
}

int ldap_version(void *ver)
{
	return LDAP_API_VERSION;
}
