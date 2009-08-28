#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <bsm/libbsm.h>
#include <bsm/audit_uevents.h>


int audit_disabled_user(const char *authedSlot, const char *targetSlot, short event)
{
	int au_cond;
	int aufd;
	token_t *tok;
	char str[1024];
	const char *action = NULL;

	// A_GETCOND data must be an int
	if (auditon(A_GETCOND, &au_cond, sizeof(int))) {
		if (errno == ENOSYS) {
			return 0;
		}
		return -1;
	}
	if (au_cond == AUC_NOAUDIT) {
		return 0;
	}

	if ((aufd = au_open()) == -1) {
		return -2;
	}

	switch(event) {
	case AUE_disable_user: action = "disabled"; break;
	case AUE_enable_user:  action = "enabled"; break;
	default:               action = "performed unknown action on";
	};

	if (authedSlot) {
		snprintf(str, sizeof(str), "%s %s %s", authedSlot, action, targetSlot);
	} else {
		snprintf(str, sizeof(str), "%s", targetSlot);
	}

	if ((tok = au_to_text(str)) == NULL) {
		return -3;
	}

	if (au_write(aufd, tok) != 0) {
		return -4;
	}

	if ((tok = au_to_return32(0, 0)) == NULL) {
		return -5;
	}

	if (au_write(aufd, tok) != 0) {
		return -6;
	}
	if (au_close(aufd, 1, event) == -1) {
		return -7;
	}

	return 0;
}

