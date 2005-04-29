/*
 * KLGraphicalUI.c
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLogin/KLGraphicalUI.c,v 1.10 2004/06/17 22:26:18 lxs Exp $
 *
 * Copyright 2004 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "KerberosAgentIPC.h"

static pid_t		gServerPID = -1;
static boolean_t	gServerKilled = false;

//
// login API
//
 
// ---------------------------------------------------------------------------

krb5_error_code __KLPrompterGUI (krb5_context  context,
                                 void         *data,
                                 const char   *name,
                                 const char   *banner,
                                 int           num_prompts,
                                 krb5_prompt   prompts[])
{
    KLStatus      result = klNoErr;
    kern_return_t err = KERN_SUCCESS;
    int           i;
    
    // Name
    KLIPCInString          ipcInName = name;
    mach_msg_type_number_t ipcInNameSize = (name == NULL) ? 0 : (strlen (name) + 1);
    
    // Banner
    KLIPCInString          ipcInBanner = banner;
    mach_msg_type_number_t ipcInBannerSize = (banner == NULL) ? 0 : (strlen (banner) + 1);
    
    // Inputs that need to allocate memory.  Must free memory if something goes wrong.
    KLIPCInString          ipcInPrompts = NULL;
    mach_msg_type_number_t ipcInPromptsSize = 0;
    KLIPCInString          ipcInHiddens = NULL;
    mach_msg_type_number_t ipcInHiddensSize = 0;   
    KLIPCInIntArray        ipcInMaxSizes = NULL;
    mach_msg_type_number_t ipcInMaxSizesSize = 0;   

    // The replies returned by the prompter:
    KLIPCOutString         ipcOutReplies = NULL;
    mach_msg_type_number_t ipcOutRepliesSize = 0;
    
    // Prompt strings
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        for (i = 0; i < num_prompts; i++) {
            ipcInPromptsSize += strlen (prompts[i].prompt) + 1;
        }
    }
    
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        ipcInPrompts = (char *) malloc (sizeof(char) * ipcInPromptsSize);
        if (ipcInPrompts == NULL) { result = klMemFullErr; }
    }
    
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        char *currentPrompt = (char *) ipcInPrompts;
        for (i = 0; i < num_prompts; i++) {
            memmove (currentPrompt, prompts[i].prompt, (strlen (prompts[i].prompt) + 1) * sizeof (char));
            currentPrompt += (strlen (prompts[i].prompt) + 1) * sizeof (char);
        }
    }
    
    // Hidden bits for prompts
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        ipcInHiddensSize = sizeof(char) * (num_prompts + 1);
        ipcInHiddens = (char *) malloc (ipcInHiddensSize);
        if (ipcInHiddens == NULL) { result = klMemFullErr; }
    }
    
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        char *hiddens = (char *) ipcInHiddens;
        for (i = 0; i < num_prompts; i++) {
            hiddens[i] = prompts[i].hidden ? '1' : '0';
        }
    }
    
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        // Max sizes for replies
        ipcInMaxSizesSize = num_prompts * sizeof (int);   
        ipcInMaxSizes = (int *) malloc (ipcInMaxSizesSize);
        if (ipcInMaxSizes == NULL) { result = klMemFullErr; }
    }
    
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        for (i = 0; i < num_prompts; i++) {
            ipcInMaxSizes[i] = prompts[i].reply->length;
        }
    }
    
        
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        SafeIPCCallBegin_ (err, result);
        err = KLIPCPrompter (machPort, applicationTask,
                             applicationPath, applicationPathLength,
                             __KLAllowHomeDirectoryAccess (),
                             ipcInName, ipcInNameSize,
                             ipcInBanner, ipcInBannerSize,
                             num_prompts,
                             ipcInPrompts, ipcInPromptsSize,
                             ipcInHiddens, ipcInHiddensSize,
                             ipcInMaxSizes, ipcInMaxSizesSize,
                             &ipcOutReplies, &ipcOutRepliesSize,
                             &result);
        SafeIPCCallEnd_ (err, result); 

        if (err != KERN_SUCCESS) { result = KLError_ (klCantContactServerErr); }
    }
    
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        char *currentReply = ipcOutReplies;

        for (i = 0; i < num_prompts; i++) {
            uint32_t replyLength = strlen (currentReply);

            if ((replyLength + 1) > prompts[i].reply->length) {
                dprintf ("__KLPrompterGUI: reply %ld is too long (is %ld, should be %ld)\n",
                         i, replyLength, prompts[i].reply->length);
                replyLength = prompts[i].reply->length;
            }
            
            memmove (prompts[i].reply->data, currentReply, replyLength + 1);
            prompts[i].reply->length = replyLength;
            
            currentReply += ipcInMaxSizes[i];
        }
    }

    if (ipcInPrompts  != NULL) { free ((void *) ipcInPrompts); }
    if (ipcInHiddens  != NULL) { free ((void *) ipcInHiddens); }
    if (ipcInMaxSizes != NULL) { free ((void *) ipcInMaxSizes); }
    if (ipcOutReplies != NULL) { vm_deallocate (mach_task_self (), (vm_address_t) ipcOutReplies, ipcOutRepliesSize); }
    
    return result;
}            

// ---------------------------------------------------------------------------
KLStatus __KLCancelAllDialogsGUI (void)
{
    KLStatus result = klCantContactServerErr;
    
    // We can't actually send an IPC to KLS because this function is called from
    // a callback while we are in the middle of a Mach-IPC request.    
    if (gServerPID > 0) {
        int err = kill (gServerPID, SIGTERM);
        if (err == 0) {
            gServerKilled = true;
            gServerPID = -1;
            result = klNoErr;
        } else {
            dprintf ("KLCancelAllDialogsGUI() failed killing KLS with err = %ld (%s)\n", 
                    errno, strerror (errno));
        }
    }
    
    return result;
}

// ---------------------------------------------------------------------------
KLStatus __KLAcquireNewInitialTicketsGUI (KLPrincipal      inPrincipal,
                                          KLLoginOptions   inLoginOptions,
                                          KLPrincipal     *outPrincipal,
                                          char           **outCredCacheName)
{
    KLStatus      result = klNoErr;
    kern_return_t err = KERN_SUCCESS;
    
    KLIPCInPrincipal        ipcInPrincipal = NULL;
    mach_msg_type_number_t  ipcInPrincipalSize = 0;
    KLIPCFlags              ipcInFlags = 0;
    KLIPCTime               ipcInLifetime = 0;
    KLIPCTime               ipcInRenewtime = 0;
    KLIPCTime               ipcInStartTime = 0;
    KLIPCBoolean            ipcInForwardable = true;
    KLIPCBoolean            ipcInProxiable = true;
    KLIPCBoolean            ipcInAddressless = true;
    KLIPCInString           ipcInServiceName = NULL;
    mach_msg_type_number_t  ipcInServiceNameSize = 0;    


    KLIPCOutPrincipal      ipcOutPrincipal = NULL;
    mach_msg_type_number_t ipcOutPrincipalSize = 0;
    KLIPCOutString         ipcOutCacheName = NULL;
    mach_msg_type_number_t ipcOutCacheNameSize = 0;
    KLPrincipal            principal = NULL;
    char                  *ccacheName = NULL;

    // Build IPC Login Options
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        if (inLoginOptions != NULL) {
            ipcInFlags       = __KLLoginOptionsGetKerberos5Options (inLoginOptions)->flags;
            ipcInLifetime    = __KLLoginOptionsGetKerberos5Options (inLoginOptions)->tkt_life;
            ipcInRenewtime   = __KLLoginOptionsGetKerberos5Options (inLoginOptions)->renew_life;
            ipcInStartTime   = __KLLoginOptionsGetStartTime (inLoginOptions);
            ipcInForwardable = __KLLoginOptionsGetKerberos5Options (inLoginOptions)->forwardable;
            ipcInProxiable   = __KLLoginOptionsGetKerberos5Options (inLoginOptions)->proxiable;
            ipcInAddressless = __KLLoginOptionsGetKerberos5Options (inLoginOptions)->address_list == NULL;
            ipcInServiceName = __KLLoginOptionsGetServiceName (inLoginOptions);
            ipcInServiceNameSize = (ipcInServiceName == NULL) ? 0 : (strlen (ipcInServiceName) + 1);
        }
    }

    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        if (inPrincipal != NULL) {
            result = KLGetStringFromPrincipal (inPrincipal, kerberosVersion_V5, (char **)&ipcInPrincipal);
            if (result == klNoErr) {
                ipcInPrincipalSize = strlen (ipcInPrincipal) + 1;
            }
        }
    }
        
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        SafeIPCCallBegin_ (err, result);
        err = KLIPCAcquireNewInitialTickets (machPort, applicationTask,
                                             applicationPath, applicationPathLength, 
                                             __KLAllowHomeDirectoryAccess (),
                                             ipcInPrincipal, ipcInPrincipalSize,
                                             ipcInFlags,
                                             ipcInLifetime, ipcInRenewtime, ipcInStartTime,
                                             ipcInForwardable, ipcInProxiable, ipcInAddressless,
                                             ipcInServiceName, ipcInServiceNameSize,
                                             &ipcOutPrincipal, &ipcOutPrincipalSize,
                                             &ipcOutCacheName, &ipcOutCacheNameSize, &result);
        SafeIPCCallEnd_ (err, result);
        if (err != KERN_SUCCESS) { result = KLError_ (klCantContactServerErr); }
    }

    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        result = __KLCreateString (ipcOutCacheName, &ccacheName);
    }
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        result = KLCreatePrincipalFromString (ipcOutPrincipal, kerberosVersion_V5, &principal);
    }

    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        if (outCredCacheName != NULL) {
            *outCredCacheName = ccacheName;
            ccacheName = NULL;
        }
        
        if (outPrincipal != NULL) {
            *outPrincipal = principal;
            principal = NULL;
        }
    }

    if (ipcInPrincipal  != NULL) { KLDisposeString ((char *)ipcInPrincipal); }
    if (ipcOutPrincipal != NULL) { vm_deallocate (mach_task_self (), (vm_address_t) ipcOutPrincipal, ipcOutPrincipalSize); }
    if (ipcOutCacheName != NULL) { vm_deallocate (mach_task_self (), (vm_address_t) ipcOutCacheName, ipcOutCacheNameSize); }
    if (principal       != NULL) { KLDisposePrincipal (principal); }
    if (ccacheName      != NULL) { KLDisposeString (ccacheName); }
    
    return result;
}

// ---------------------------------------------------------------------------
KLStatus __KLChangePasswordGUI (KLPrincipal inPrincipal)
{
    KLStatus               result = klNoErr;
    kern_return_t          err = KERN_SUCCESS;
    KLIPCInPrincipal       ipcInPrincipal = NULL;
    mach_msg_type_number_t ipcInPrincipalSize = 0;

    if (inPrincipal != NULL) {
        result = KLGetStringFromPrincipal (inPrincipal, kerberosVersion_V5, (char **)&ipcInPrincipal);
        if (result == klNoErr) {
            ipcInPrincipalSize = strlen (ipcInPrincipal) + 1;
        }
    }
    
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        SafeIPCCallBegin_ (err, result);
        err = KLIPCChangePassword (machPort, applicationTask,
                                   applicationPath, applicationPathLength,
                                   __KLAllowHomeDirectoryAccess (),
                                   ipcInPrincipal, ipcInPrincipalSize, &result);
        SafeIPCCallEnd_ (err, result);
        if (err != KERN_SUCCESS) { result = KLError_ (klCantContactServerErr); }
    }
    

    if (ipcInPrincipal != NULL) KLDisposeString ((char *)ipcInPrincipal);

    
    return result;
}    

// ---------------------------------------------------------------------------
KLStatus __KLHandleErrorGUI (KLStatus inError, 
                             KLDialogIdentifier inDialogIdentifier, 
                             KLBoolean inShowAlert)
{
    KLStatus      result = klNoErr;
    kern_return_t err = KERN_SUCCESS;

    SafeIPCCallBegin_ (err, result);
    err = KLIPCHandleError (machPort, applicationTask, 
                            applicationPath, applicationPathLength,
                            __KLAllowHomeDirectoryAccess (),
                            inError, inDialogIdentifier, inShowAlert, &result);
    SafeIPCCallEnd_ (err, result);

    if (err != KERN_SUCCESS) { result = KLError_ (klCantContactServerErr); }
    
    return result;
}
