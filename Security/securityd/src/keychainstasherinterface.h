#ifndef keychainstasherinterface_h
#define keychainstasherinterface_h

#ifdef __cplusplus
extern "C" {
#endif

OSStatus stashKeyWithStashAgent(uid_t client, void const* keybytes, size_t keylen);
OSStatus loadKeyFromStashAgent(uid_t client, void** keybytes, size_t* keylen);

#ifdef __cplusplus
}
#endif

#endif /* keychainstasherinterface_h */
