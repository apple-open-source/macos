//
//  tool_auth_helpers.h
//  Security
//

#ifndef tool_auth_helpers_h
#define tool_auth_helpers_h

bool checkPassphrase(char* passphrase, rsize_t len);
bool promptForAndCheckPassphrase(void);
bool authRequired(void);

#endif /* tool_auth_helpers_h */
