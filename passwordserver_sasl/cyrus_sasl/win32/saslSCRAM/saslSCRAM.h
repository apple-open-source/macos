#ifndef _SASLSCRAM_H_
#define _SASLSCRAM_H_

// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the SASLSCRAM_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// SASLSCRAM_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifdef SASLSCRAM_EXPORTS
#define SASLSCRAM_API __declspec(dllexport)
#else
#define SASLSCRAM_API __declspec(dllimport)
#endif

SASLSCRAM_API int sasl_server_plug_init(sasl_utils_t *utils, int maxversion,
			  int *out_version,
			  const sasl_server_plug_t **pluglist,
			  int *plugcount);

SASLSCRAM_API int sasl_client_plug_init(sasl_utils_t *utils, int maxversion,
			  int *out_version, const sasl_client_plug_t **pluglist,
			  int *plugcount);

#endif /* _SASLSCRAM_H_ */
