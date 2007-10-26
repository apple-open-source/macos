/*	UNCUserNotification.h
	Copyright 2000, Apple Computer, Inc. All rights reserved.
*/

#ifndef _UNCUSERNOTIFICATION_H_
#define _UNCUSERNOTIFICATION_H_

typedef struct __UNCUserNotification *UNCUserNotificationRef;

/* Create and dispatch a notification, providing contents as a null-terminated list of key-value pairs */
extern UNCUserNotificationRef UNCUserNotificationCreate(double timeout, unsigned long flags, int *error, const char **contents);

/* Wait for a response */
extern int UNCUserNotificationReceiveResponse(UNCUserNotificationRef userNotification, double timeout, unsigned long *responseFlags);

/* Retrieve contents (if any) from the response */
extern const char *UNCUserNotificationGetResponseValue(UNCUserNotificationRef userNotification, const char *key, unsigned long index);

/* Retrieve all response contents as a null-terminated list of key-value pairs */
extern const char **UNCUserNotificationGetResponseContents(UNCUserNotificationRef userNotification);

/* Update the values associated with a notification */
extern int UNCUserNotificationUpdate(UNCUserNotificationRef userNotification, double timeout, unsigned long flags, const char **contents);

/* Cancel a notification */
extern int UNCUserNotificationCancel(UNCUserNotificationRef userNotification);

/* Free the storage associated with a notification */
extern void UNCUserNotificationFree(UNCUserNotificationRef userNotification);


/* Convenience APIs for the simplest cases */
extern int UNCDisplayNotice(double timeout, unsigned long flags, const char *iconPath, const char *soundPath, const char *localizationPath, const char *alertHeader, const char *alertMessage, const char *defaultButtonTitle);

extern int UNCDisplayAlert(double timeout, unsigned long flags, const char *iconPath, const char *soundPath, const char *localizationPath, const char *alertHeader, const char *alertMessage, const char *defaultButtonTitle, const char *alternateButtonTitle, const char *otherButtonTitle, unsigned long *responseFlags);


/* Flags */

enum {
    kUNCStopAlertLevel		= 0,
    kUNCNoteAlertLevel		= 1,
    kUNCCautionAlertLevel	= 2,
    kUNCPlainAlertLevel		= 3
};

enum {
    kUNCDefaultResponse		= 0,
    kUNCAlternateResponse	= 1,
    kUNCOtherResponse		= 2,
    kUNCCancelResponse		= 3
};

enum {
    kUNCNoDefaultButtonFlag 	= (1 << 5),
    kUNCUseRadioButtonsFlag 	= (1 << 6)
};

#define UNCCheckBoxChecked(i)	(1 << (8 + i))
#define UNCSecureTextField(i)	(1 << (16 + i))
#define UNCPopUpSelection(n)	(n << 24)



/* Keys to be used in message contents */
/* - keys to be used at most once */
extern const char kUNCIconPathKey[];
extern const char kUNCSoundPathKey[];
extern const char kUNCLocalizationPathKey[];
extern const char kUNCAlertHeaderKey[];
extern const char kUNCDefaultButtonTitleKey[];
extern const char kUNCAlternateButtonTitleKey[];
extern const char kUNCOtherButtonTitleKey[];
extern const char kUNCProgressIndicatorValueKey[];
/* - keys that may be used more than once in a row */
extern const char kUNCAlertMessageKey[];
extern const char kUNCPopUpTitlesKey[];
extern const char kUNCTextFieldTitlesKey[];
extern const char kUNCCheckBoxTitlesKey[];
extern const char kUNCTextFieldValuesKey[];
extern const char kUNCPopUpSelectionKey[];

#endif	/* _UNCUSERNOTIFICATION_H_ */
