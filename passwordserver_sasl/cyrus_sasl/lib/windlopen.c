/* windlopen.c--Windows dynamic loader interface
 * Ryan Troll
 * $Id: windlopen.c,v 1.2 2002/05/22 17:56:56 snsimon Exp $
 */
/* 
 * Copyright (c) 2001 Carnegie Mellon University.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any other legal
 *    details, please contact  
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <stdio.h>

#include <config.h>
#include <sasl.h>

int _sasl_get_plugin(const char *file,
		     const char *entryname,
		     const sasl_callback_t *getpath_callback,
		     const sasl_callback_t *verifyfile_callback,
		     void **entrypoint,
		     void **library)
{
    /* not supported */
    return SASL_FAIL;
}

/* gets the list of mechanisms */
int _sasl_get_mech_list(const char *entryname,
			const sasl_callback_t *getpath_cb,
			const sasl_callback_t *verifyfile_cb,
			int (*add_plugin)(void *,void *))
{

  /* Open registry entry, and find all registered SASL libraries.
   *
   * Registry location:
   *
   *     SOFTWARE\\Carnegie Mellon\\Project Cyrus\\SASL Library\\Available Plugins
   *
   * Key - value:
   *
   *     "Cool Name" - "c:\sasl\plugins\coolname.dll"
   */

#define MAX_VALUE_NAME              128

  HKEY  hKey;
  int   Index;
  CHAR  ValueName[MAX_VALUE_NAME];
  DWORD dwcValueName = MAX_VALUE_NAME;
  CHAR  Location[MAX_PATH];
  DWORD dwcLocation = MAX_PATH;
  DWORD ret;

  /* Open the registry 
   */
  ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
    SASL_KEY,
    0,
    KEY_READ,
    &hKey);

  if (ret != ERROR_SUCCESS) { return SASL_FAIL; }

  // Now enumerate across all registry keys.

  for (Index = 0; ret == ERROR_SUCCESS; Index++) {

      dwcLocation = MAX_PATH;
      dwcValueName = MAX_VALUE_NAME;
    ret= RegEnumValue (hKey, Index, ValueName, &dwcValueName,
      NULL, // Reserved,
      NULL, // Type,
      Location, &dwcLocation);

    if (ret == ERROR_SUCCESS) {

      /*
       * ValueName: "Really Cool Plugin"
       * Location: "c:\sasl\plugins\cool.dll"
       */
      HINSTANCE library    = NULL;
      FARPROC entry_point = NULL;

      /* Found a library.  Now open it.
       */
      library = LoadLibrary(Location);
      if (library == NULL) {
        DWORD foo = GetLastError();
	_sasl_log(NULL, SASL_LOG_ERR, "Unable to dlopen %s: %d",
		  Location, foo);
        continue;
      }

      /* Opened the library.  Find the entrypoint
       */
      entry_point = GetProcAddress(library, entryname);
        
      if (entry_point == NULL) {
        _sasl_log(NULL, SASL_LOG_ERR,
		  "can't get entry point %s: %d", entryname, GetLastError());
        FreeLibrary(library);
        continue;
      }

      /* Opened the library, found the entrypoint.  Now add it.
       */
      if ((*add_plugin)(entry_point, (void *)library) != SASL_OK) {
        _sasl_log(NULL, SASL_LOG_ERR,
		  "add_plugin to list failed");
        FreeLibrary(library);
        continue;
      }
    }
  } /* End of registry value loop */

  RegCloseKey(hKey);

  return SASL_OK;
}






int
_sasl_done_with_plugin(void *plugin)
{
  if (! plugin)
    return SASL_BADPARAM;

  FreeLibrary((HMODULE)plugin);

  return SASL_OK;
}
