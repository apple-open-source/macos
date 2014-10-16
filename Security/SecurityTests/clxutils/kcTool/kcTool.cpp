#include <Security/SecKeychainItem.h>
#include <Security/SecKeychain.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/param.h>

#define KC_DB_PATH			"Library/Keychains"		/* relative to home */

static void usage(char **argv)
{
	printf("usage: %s keychainName command [options]\n", argv[0]); 
	printf("Commands:\n");
	printf("   c  create\n");
	printf("   s  get status\n");
	printf("   l  lock\n");
	printf("   u  unlock\n");
	printf("   a  add genericPassword\n");
	printf("   g  get (lookup) genericPassword\n");
	printf("   d  delete genericPassword\n");
	printf("Options:\n");
	printf("   p=keychainPassword\n");
	printf("   g=genericPassword\n");
	printf("Options (for create only):\n");
	printf("   l=lockIntervalInSeconds\n");
	printf("   L (no lockOnSleep)\n");
	printf("   n(o user prompt)\n");
	printf("   h(elp)\n");
	exit(1);
}

/*
 * For add/search generic password 
 */
#define GP_SERVICE_NAME			"kctool"
#define GP_SERVICE_NAME_LEN		((UInt32)strlen(GP_SERVICE_NAME))
#define GP_ACCOUNT_NAME			"John Galt"
#define GP_ACCOUNT_NAME_LEN		((UInt32)strlen(GP_ACCOUNT_NAME))

typedef enum {
	KC_Nop,
	KC_CreateKC,
	KC_GetStatus,
	KC_LockKC,
	KC_UnlockKC,
	KC_AddPasswd,
	KC_LookupPasswd,
	KC_DeletePasswd
} KcOp;

static void showError(
	OSStatus ortn,
	const char *msg)
{
	printf("***Error %d on %s.\n", (int)ortn, msg);
}

static void safePrint(
	char *buf, 
	UInt32 len)
{
	UInt32 i;
	char c;
	
	for(i=0; i<len; i++) {
		c = *buf++;
		if(c == '\0') {
			break;
		}
		putchar(c);
	}
}

int main(int argc, char **argv)
{
	SecKeychainRef 		kcRef = nil;
	char 				kcPath[MAXPATHLEN + 1];
	OSStatus 			ortn;
	int					arg;
	char				*argp;
	
	/* command line arguments */
	KcOp				op = KC_Nop;
	char				*kcPwd = NULL;
	UInt32				kcPwdLen = 0;
	char				*genericPwd = NULL;
	UInt32				genericPwdLen = 0;
	int					lockInterval = 0;
	Boolean				noLockOnSleep = false;
	Boolean				userPrompt = true;
	
	if(argc < 3) {
		usage(argv);
	}
	
	switch(argv[2][0]) {
		case 'c':
			op = KC_CreateKC;
			break;
		case 's':
			op = KC_GetStatus;
			break;
		case 'l':
			op = KC_LockKC;
			break;
		case 'u':
			op = KC_UnlockKC;
			break;
		case 'a':
			op = KC_AddPasswd;
			break;
		case 'g':
			op = KC_LookupPasswd;
			break;
		case 'd':
			op = KC_DeletePasswd;
			break;
		default:
			usage(argv);
	}
	for(arg=3; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'p':
				kcPwd = &argp[2];
				kcPwdLen = strlen(kcPwd);
				break;
			case 'g':
				genericPwd = &argp[2];
				genericPwdLen = strlen(genericPwd);
				break;
			case 'l':
				lockInterval = atoi(&argp[2]);
				break;
			case 'L':
				noLockOnSleep = true;
				break;
			case 'n':
				userPrompt = false;
				break;
			default:
				usage(argv);
		}
	}
	/* cook up KC path */
	if(argv[1][0] == '/') {
		/* absolute path already given */
		strcpy(kcPath, argv[1]);
	}
	else {
		char *userHome = getenv("HOME");
		if(userHome == NULL) {
			/* well, this is probably not going to work */
			userHome = (char *)"";
		}
		sprintf(kcPath, "%s/%s/%s", userHome, KC_DB_PATH, argv[1]);
	}
	
	/* all commands except KC_CreateKC: open specified keychain */
	if(op != KC_CreateKC) {
		ortn = SecKeychainOpen(kcPath, &kcRef);
		if(ortn) {
			showError(ortn, "SecKeychainOpen");
			printf("Cannot open keychain at %s. Aborting.\n", kcPath);
			exit(1);
		}
	}
	
	/* execute */
	switch(op) {
		case KC_CreateKC:
		{
			ortn = SecKeychainCreate(kcPath,
				kcPwdLen,		// may be 0
				kcPwd,			// may be NULL
				userPrompt,
				nil,			// initialAccess
				&kcRef);
			if(ortn) {
				showError(ortn, "SecKeychainCreateNew");
				exit(1);				
			}
			else {
				printf("...keychain %s created.\n", argv[1]);
			}
			break;
		}
		case KC_GetStatus:
		{
			SecKeychainStatus kcStat;
			ortn = SecKeychainGetStatus(kcRef, &kcStat);
			if(ortn) {
				showError(ortn, "SecKeychainGetStatus");
				exit(1);				
			}
			printf("...SecKeychainStatus = %u ( ", (unsigned)kcStat);
			if(kcStat & kSecUnlockStateStatus) {
				printf("UnlockState ");
			}
			if(kcStat & kSecReadPermStatus) {
				printf("RdPerm ");
			}
			if(kcStat & kSecWritePermStatus) {
				printf("WrPerm ");
			}
			printf(")\n");
			break;
		}
		case KC_LockKC:
		{
			ortn = SecKeychainLock(kcRef);
			if(ortn) {
				showError(ortn, "SecKeychainLock");
				exit(1);				
			}
			else {
				printf("...keychain %s locked.\n", argv[1]);
			}
			break;
		}
		case KC_UnlockKC:
		{
			ortn = SecKeychainUnlock(kcRef,
				kcPwdLen,
				kcPwd,
				kcPwd ? true : false);
			if(ortn) {
				showError(ortn, "SecKeychainUnlock");
				exit(1);				
			}
			else {
				printf("...keychain %s unlocked.\n", argv[1]);
			}
			break;
		}
		case KC_AddPasswd:
		{
			SecKeychainItemRef itemRef = nil;
			if(genericPwd == NULL) {
				printf("***Must supply a genericPassword argument.\n");
				exit(1); 
			}
			ortn = SecKeychainAddGenericPassword(kcRef,
				GP_SERVICE_NAME_LEN, GP_SERVICE_NAME, 
				GP_ACCOUNT_NAME_LEN, GP_ACCOUNT_NAME, 
				genericPwdLen, genericPwd, 
				&itemRef);
			if(ortn) {
				showError(ortn, "SecKeychainAddGenericPassword");
				exit(1);				
			}
			else {
				printf("...password added to keychain %s.\n", argv[1]);
			}
			break;
		}
		case KC_LookupPasswd:
		case KC_DeletePasswd:
		{
			char *foundPassword;
			UInt32 pwdLen;
			SecKeychainItemRef itemRef = nil;
			ortn = SecKeychainFindGenericPassword(kcRef,
				GP_SERVICE_NAME_LEN, GP_SERVICE_NAME, 
				GP_ACCOUNT_NAME_LEN, GP_ACCOUNT_NAME, 
				&pwdLen, (void **)&foundPassword, 
				&itemRef);
			if(ortn) {
				showError(ortn, "SecKeychainFindGenericPassword");
				exit(1);				
			}
			else if(op == KC_DeletePasswd) {
				/* found it, now delete it */
				ortn = SecKeychainItemDelete(itemRef);
				if(ortn) {
					showError(ortn, "SecKeychainItemDelete");
					exit(1);				
				}
				else {
					printf("...generic password deleted.\n");
				}
			}
			else {
				printf("...password found: ");
				safePrint(foundPassword, pwdLen);
				printf("\n");
			}
			break;
		}
		default:
			usage(argv);
	}
	/* CLEANUP */
	return 0;
}


