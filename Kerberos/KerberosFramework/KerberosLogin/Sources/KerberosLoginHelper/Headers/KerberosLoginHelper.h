#include <Kerberos/KerberosLogin.h>
#include <Kerberos/krb5.h>

#if TARGET_OS_MAC
    #if defined(__MWERKS__)
        #pragma import on
        #pragma enumsalwaysint on
    #endif
    #pragma options align=mac68k
#endif

enum {
	/* Kerberos Login Helper Event Class */
	kKLHEventClass					 = 'logn',

	/* Events in the Kerberos Login Helper Event Class */
	kAEAcquireNewInitialTickets		 = 'acqn',
	kAEChangePassword				 = 'chps',
	kAEHandleError					 = 'herr',
	kAECancelAllDialogs				 = 'cads',
	kAEPrompter						 = 'prom',

	/* Custom AppleEvent keys */
	keyKLPrincipal					 = FOUR_CHAR_CODE('prin'),
	keyKLCacheName					 = FOUR_CHAR_CODE('cach'),
	keyKLError						 = FOUR_CHAR_CODE('err '),
	keyKLDialogIdentifier			 = FOUR_CHAR_CODE('dgid'),
	keyKLShowAlert					 = FOUR_CHAR_CODE('bool'),
	keyKLPrompterContext			 = FOUR_CHAR_CODE('pctx'),
	keyKLPrompterName				 = FOUR_CHAR_CODE('pnam'),
	keyKLPrompterBanner				 = FOUR_CHAR_CODE('pban'),
	keyKLPrompterNumPrompts			 = FOUR_CHAR_CODE('pnum'),
	keyKLPrompterPromptStrings		 = FOUR_CHAR_CODE('pstr'),
	keyKLPrompterPromptHidden		 = FOUR_CHAR_CODE('phid'),
	keyKLPrompterReplyMaxSizes		 = FOUR_CHAR_CODE('psiz'),
	keyKLPrompterReplies			 = FOUR_CHAR_CODE('prep'),
	
	/* Custom AppleEvent types */
	typeKLPrincipalString			 = typeChar,
	typeKLCacheName					 = typeChar,
	typeKLPrompterName				 = typeChar,
	typeKLPrompterBanner			 = typeChar,
	typeKLPrompterNumPrompts		 = typeSInt32,
	typeKLPrompterStrings			 = typeChar,
	typeKLPrompterBooleans			 = typeChar,
	typeKLPrompterMaxSizes			 = FOUR_CHAR_CODE('msiz'),
	typeKLPrompterReplies			 = typeChar
};

KLStatus KLHAcquireInitialNewTickets (KLPrincipal inPrincipal, KLPrincipal* outPrincipal, char** outCacheName);
KLStatus KLHChangePassword (KLPrincipal inPrincipal);
KLStatus KLHHandleError (KLStatus inError, KLDialogIdentifier inDialogIdentifier, KLBoolean inShowAlert);
KLStatus KLHCancelAllDialogs ();
krb5_error_code KLHPrompter (
			krb5_context	context,
			void 			*data,
	const	char			*name,
	const	char			*banner,
			int				num_prompts,
			krb5_prompt		prompts[]);

#if TARGET_OS_MAC
    #if defined(__MWERKS__)
        #pragma enumsalwaysint reset
        #pragma import reset
    #endif
	#pragma options align=reset
#endif
