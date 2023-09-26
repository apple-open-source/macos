#ifndef BINTRANS_H
#define	BINTRANS_H

#ifndef nitems
#define	nitems(x)	(sizeof((x)) / sizeof((x)[0]))
#endif

size_t apple_b64_ntop(u_char const *src, size_t srclength, char *target,
    size_t targsize);
int apple_b64_pton(const char *src, u_char *target, size_t targsize);

#endif	/* BINTRANS_H */
