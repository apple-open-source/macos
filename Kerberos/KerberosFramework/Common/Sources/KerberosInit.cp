#include <Kerberos/KerberosDebug.h>
#include <CoreFoundation/CoreFoundation.h>
#include "KerberosInit.h"

extern "C" {
#include "crypto_libinit.h"
#include "krb5_libinit.h"
#include "gss_libinit.h"

#if USE_HARDCODED_FALLBACK_ERROR_TABLES
#include <Kerberos/com_err.h>
#include "Apple Event Manager.h"
#include "Code Fragment Manager.h"
#include "File Manager.h"
#include "Gestalt Manager.h"
#include "Memory Manager.h"
#include "Process Manager.h"
#include "Resource Manager.h"
#include "Thread Manager.h"
#include "KerberosLoginErrors.h"
#include "krb_err.h"
#include "prof_err.h"
#include "krb524_err.h"
#include "KClientErrors.h"
#include "KClientCompatErrors.h"
#endif
}

KerberosFrameworkInitializer gKerberosFrameworkInitializer;

KerberosFrameworkInitializer::KerberosFrameworkInitializer ()
{
    cryptoint_initialize_library ();    
    krb5int_initialize_library ();
    gssint_initialize_library ();

#if USE_HARDCODED_FALLBACK_ERROR_TABLES
    // Load the error tables in case the strings don't work:
    add_error_table (&et_Appl_error_table);	 // Apple Event Manager
    add_error_table (&et_Code_error_table);  // Code Fragment Manager
    add_error_table (&et_File_error_table);  // File Manager
    add_error_table (&et_Gest_error_table);  // Gestalt Manager
    add_error_table (&et_Memo_error_table);  // Memory Manager
    add_error_table (&et_Proc_error_table);  // Process Manager
    add_error_table (&et_Reso_error_table);  // Resource Manager
    add_error_table (&et_Thre_error_table);  // Thread Manager
    add_error_table (&et_krb_error_table);   // Kerberos4
	add_error_table (&et_prof_error_table);  // KerberosProfile
    add_error_table (&et_k524_error_table);  // Kerberos524
	add_error_table (&et_KLL_error_table);   // KerberosLogin
    add_error_table (&et_KCli_error_table);  // KClient
    add_error_table (&et_KCpt_error_table);  // KClientCompat
#endif
}

KerberosFrameworkInitializer::~KerberosFrameworkInitializer ()
{
    // Terminate API components in reverse order    
#if USE_HARDCODED_FALLBACK_ERROR_TABLES
    remove_error_table (&et_Appl_error_table);	// Apple Event Manager
    remove_error_table (&et_Code_error_table);  // Code Fragment Manager
    remove_error_table (&et_File_error_table);  // File Manager
    remove_error_table (&et_Gest_error_table);  // Gestalt Manager
    remove_error_table (&et_Memo_error_table);  // Memory Manager
    remove_error_table (&et_Proc_error_table);  // Process Manager
    remove_error_table (&et_Reso_error_table);  // Resource Manager
    remove_error_table (&et_Thre_error_table);  // Thread Manager
    remove_error_table (&et_krb_error_table);   // Kerberos4
	remove_error_table (&et_prof_error_table);  // KerberosProfile 
    remove_error_table (&et_k524_error_table);  // Kerberos524
	remove_error_table (&et_KLL_error_table);   // KerberosLogin
    remove_error_table (&et_KCli_error_table);  // KClient
    remove_error_table (&et_KCpt_error_table);  // KClientCompat
#endif
    
    gssint_cleanup_library ();
 	krb5int_cleanup_library ();
    cryptoint_cleanup_library ();   
}
