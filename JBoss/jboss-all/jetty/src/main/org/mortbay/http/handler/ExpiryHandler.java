// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: ExpiryHandler.java,v 1.1.4.5 2003/06/04 04:47:46 starksm Exp $
// ========================================================================

package org.mortbay.http.handler;

import java.io.IOException;
import java.util.Date;
import org.mortbay.http.HttpException;
import org.mortbay.http.HttpFields;
import org.mortbay.http.HttpRequest;
import org.mortbay.http.HttpResponse;
import org.mortbay.util.Code;

/* ------------------------------------------------------------ */
/**
 * Handler that allows the default Expiry of all content to be set.
 *
 * @version $Id: ExpiryHandler.java,v 1.1.4.5 2003/06/04 04:47:46 starksm Exp $
 * @author Brett Sealey
 */
public class ExpiryHandler extends AbstractHttpHandler
{
    /**
     * The default expiry time in seconds
     */
    private long _ttl=-1;

    /* ------------------------------------------------------------ */
    /**
     * Set the default expiry time in seconds.
     *
     * @param ttl The default time to live in seconds. If negative (the
     * default) then all content will be set to expire 01Jan1970 by default.
     */
    public void setTimeToLive(long ttl)
    {
        _ttl=ttl;
    }

    /* ------------------------------------------------------------ */
    /** Handle a request by pre-populating the Expires header with a a value
     * that corresponds to now + ttl. If ttl -s negative then
     * HttpFields.__01Jan1970 is used.
     *
     * Settings made here can be overridden by subsequent handling of the
     * request.
     *
     * @param pathInContext The context path
     * @param pathParams Path parameters such as encoded Session ID
     * @param request The HttpRequest request
     * @param response The HttpResponse response
     */
    public void handle(String pathInContext,
                       String pathParams,
                       HttpRequest request,
                       HttpResponse response)
            throws HttpException,IOException
    {
        Code.debug("ExpiryHandler.handle()");
        String expires;
        if (_ttl<0)
            expires=HttpFields.__01Jan1970;
        else
            expires=HttpFields.__dateSend
                    .format(new Date(System.currentTimeMillis()+1000L*_ttl));
        response.setField(HttpFields.__Expires,expires);
    }
}
