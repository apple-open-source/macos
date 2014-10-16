#ifndef	_PRINT_CERT_NAME_H_
#define _PRINT_CERT_NAME_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Print subject and/or issuer of a cert.
 */
typedef enum {
    NameBoth = 0,
    NameSubject,
    NameIssuer
} WhichName;

extern void printCertName(
    const unsigned char *cert,
    unsigned certLen,
    WhichName whichName);

#ifdef __cplusplus
}
#endif

#endif	/* _PRINT_CERT_NAME_H_ */

