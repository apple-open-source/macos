#ifndef _SASLKERBEROSV4_H_
#define _SASLKERBEROSV4_H_

// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the SASLKERBEROSV4_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// SASLKERBEROSV4_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifdef SASLKERBEROSV4_EXPORTS
#define SASLKERBEROSV4_API __declspec(dllexport)
#else
#define SASLKERBEROSV4_API __declspec(dllimport)
#endif

SASLKERBEROSV4_API int sasl_server_plug_init(sasl_utils_t *utils, int maxversion,
			  int *out_version,
			  const sasl_server_plug_t **pluglist,
			  int *plugcount);

SASLKERBEROSV4_API int sasl_client_plug_init(sasl_utils_t *utils, int maxversion,
			  int *out_version, const sasl_client_plug_t **pluglist,
			  int *plugcount);

#endif /* _SASLKERBEROSV4_H_ */

