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


/*! @functiongroup group_1 */

/*! @function _maintRequest1
    @abstract Synchrounous implementation of $link addEventSource & $link removeEventSource functions.
    @discussion Test of unnamed parameters. */
    virtual IOReturn _maintRequest(void *command, void *data, void *, void *);

/*! @function test1
    @abstract Test of vararg.
    @param foo Format string.
    @param ... additional parameters. */
    virtual IOReturn test(const char *foo,...);

/*! @functiongroup group_2
*/

/*! @function _maintRequest2
    @abstract Synchrounous implementation of $link addEventSource & $link removeEventSource functions.
    @discussion Test of unnamed parameters. */
    virtual IOReturn _maintRequest(void *command, void *data, void *, void *);

/*! @function test2
    @abstract Test of vararg.
    @param foo Format string.
    @param ... additional parameters. */
    virtual IOReturn test(const char *foo,...);

/*! @functiongroup group_0 */

/*! @function _maintRequest3
    @abstract Synchrounous implementation of $link addEventSource & $link removeEventSource functions.
    @discussion Test of unnamed parameters. */
    virtual IOReturn _maintRequest(void *command, void *data, void *, void *);

/*! @function test3
    @abstract Test of vararg.
    @param foo Format string.
    @param ... additional parameters. */
    virtual IOReturn test(const char *foo,...);

/*! @functiongroup group_1
*/

/*! @function _maintRequest4
    @abstract Synchrounous implementation of $link addEventSource & $link removeEventSource functions.
    @discussion Test of unnamed parameters. */
    virtual IOReturn _maintRequest(void *command, void *data, void *, void *);

/*! @function test4
    @abstract Test of vararg.
    @param foo Format string.
    @param ... additional parameters. */
    virtual IOReturn test(const char *foo,...);

