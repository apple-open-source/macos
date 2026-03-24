//
//  PluginSafePath.h
//  authd
//

/*!
    @function plugin_stage
    @abstract Securely stages or removes authorization plugins for safe execution
    @discussion This function handles the staging of authorization plugins by copying them to a secure location
                or removing previously staged plugins. The function validates input parameters, prevents path
                traversal attacks, and manages plugin lifecycle per process.
    @param conn The XPC connection from the requesting process
    @param message XPC message containing plugin path (for staging) and operation type
    @param reply XPC reply object to send response back to client
    @result OSStatus indicating success or specific error condition
*/

OSStatus
plugin_stage(pid_t instance, const char  * _Nonnull originalPath, char *_Nullable * _Nonnull safePath);

OSStatus
plugin_unstage(pid_t instance);

Boolean
plugin_stagedir_clean(void);

