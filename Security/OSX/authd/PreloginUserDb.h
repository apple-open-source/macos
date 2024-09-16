//
//  PreloginUserDb.h
//  authd
//

OSStatus preloginudb_copy_userdb(const char * _Nullable uuid, UInt32 flags, CFArrayRef _Nonnull * _Nonnull output);
OSStatus prelogin_copy_pref_value(const char * _Nullable uuid, const char * _Nullable user, const char * _Nonnull domain, const char * _Nonnull item, CFTypeRef _Nonnull * _Nonnull output);
OSStatus prelogin_smartcardonly_override(const char * _Nullable uuid, unsigned char operation, Boolean * _Nullable status);
Boolean prelogin_reset_db_wanted(void);

Boolean pssoEnabled(void);

CF_RETURNS_RETAINED CFDateRef _Nonnull dateFromUnixTimestamp(__darwin_time_t seconds);
CF_RETURNS_RETAINED CFDictionaryRef _Nullable readDatabasePlist(CFStringRef _Nonnull path, CFErrorRef _Nullable * _Nullable error);
