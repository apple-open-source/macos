#ifndef _SASLPLAIN_H_
#define _SASLPLAIN_H_

// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the SASLPLAIN_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// SASLPLAIN_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifdef SASLPLAIN_EXPORTS
#define SASLPLAIN_API __declspec(dllexport)
#else
#define SASLPLAIN_API __declspec(dllimport)
#endif

SASLPLAIN_API int sasl_server_plug_init(sasl_utils_t *utils, int maxversion,
			  int *out_version,
			  const sasl_server_plug_t **pluglist,
			  int *plugcount);

SASLPLAIN_API int sasl_client_plug_init(sasl_utils_t *utils, int maxversion,
			  int *out_version, const sasl_client_plug_t **pluglist,
			  int *plugcount);

#endif /* _SASLPLAIN_H_ */
