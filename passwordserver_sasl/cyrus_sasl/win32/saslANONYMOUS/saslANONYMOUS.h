#ifndef _SASLANONYMOUS_H_
#define _SASLANONYMOUS_H_

// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the SASLANONYMOUS_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// SASLANONYMOUS_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifdef SASLANONYMOUS_EXPORTS
#define SASLANONYMOUS_API __declspec(dllexport)
#else
#define SASLANONYMOUS_API __declspec(dllimport)
#endif

SASLANONYMOUS_API int sasl_server_plug_init(sasl_utils_t *utils, int maxversion,
			  int *out_version,
			  const sasl_server_plug_t **pluglist,
			  int *plugcount);

SASLANONYMOUS_API int sasl_client_plug_init(sasl_utils_t *utils, int maxversion,
			  int *out_version, const sasl_client_plug_t **pluglist,
			  int *plugcount);

#endif /* _SASLANONYMOUS_H_ */

