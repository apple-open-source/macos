/*! @header Function.h
    @discussion This header tests the supported types of functions declarations.  
*/

/*! @function _maintRequest
    @abstract Synchrounous implementation of $link addEventSource & $link removeEventSource functions.
    @discussion Test of unnamed parameters. */
    virtual IOReturn _maintRequest(void *command, void *data, void *, void *);

/*! @function test
    @abstract Test of vararg.
    @param foo Format string.
    @param ... additional parameters. */
    virtual IOReturn test(const char *foo,...);

