#ifndef KEY_H
#define KEY_H

#include <openssl/rsa.h>
#include <openssl/dsa.h>

typedef struct Key Key;
enum types {
	KEY_RSA,
	KEY_DSA,
	KEY_EMPTY
};
struct Key {
	int	type;
	RSA	*rsa;
	DSA	*dsa;
};

Key	*PS_key_new(int type);
void	PS_key_free(Key *k);
int	PS_key_equal(Key *a, Key *b);
char	*PS_key_fingerprint(Key *k);
char	*PS_key_type(Key *k);
int	PS_key_write(Key *key, FILE *f);
unsigned int PS_key_read(Key *key, char **cpp);

#endif
