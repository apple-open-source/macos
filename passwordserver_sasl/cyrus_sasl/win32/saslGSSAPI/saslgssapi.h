#ifndef _SASLGSSAPI_H_
#define _SASLGSSAPI_H_

// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the SASLGSSAPI_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// SASLGSSAPI_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifdef SASLGSSAPI_EXPORTS
#define SASLGSSAPI_API __declspec(dllexport)
#else
#define SASLGSSAPI_API __declspec(dllimport)
#endif

SASLGSSAPI_API int sasl_server_plug_init(sasl_utils_t *utils, int maxversion,
			  int *out_version,
			  const sasl_server_plug_t **pluglist,
			  int *plugcount);

SASLGSSAPI_API int sasl_client_plug_init(sasl_utils_t *utils, int maxversion,
			  int *out_version, const sasl_client_plug_t **pluglist,
			  int *plugcount);

#endif /* _SASLGSSAPI_H_ */
