/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.http;

import java.lang.reflect.Array;

import java.io.Serializable;

import org.jboss.logging.Logger;

import org.jboss.mq.ReceiveRequest;
import org.jboss.mq.SpyDestination;
import org.jboss.mq.il.ClientIL;

/**
 * The HTTP/S implementation of the ClientIL object
 *
 * @author    Nathan Phelps (nathan@jboss.org)
 * @version   $Revision: 1.1.2.1 $
 * @created   January 15, 2003
 */
public class HTTPClientIL implements ClientIL, Serializable
{
    public boolean stopped = true;
    
    private static Logger log = Logger.getLogger(HTTPClientIL.class);
    private String clientIlId = null;
    
    public HTTPClientIL(String clientIlId)
    {
        this.clientIlId = clientIlId;
        if (log.isTraceEnabled())
        {
            log.trace("created(" + clientIlId + ")");
        }
    }
    
    public void close() throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("close()");
        }
        this.throwIllegalStateExceptionIfStopped();
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("asynchClose");
        HTTPClientILStorageQueue.getInstance().put(request, this.clientIlId);
    }
    
    public void deleteTemporaryDestination(SpyDestination dest) throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("deleteTemporaryDestination(SpyDestination " + dest.toString() + ")");
        }
        this.throwIllegalStateExceptionIfStopped();
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("asynchDeleteTemporaryDestination");
        request.setArguments(new Object[]
        {dest}, new Class[]
        {SpyDestination.class});
        HTTPClientILStorageQueue.getInstance().put(request, this.clientIlId);
    }
    
    public void pong(long serverTime) throws Exception
    {
        // We don't do pings/pongs so this will never get called, but heck, just in case...
        log.warn("Pong was called by the server.  The Ping value for this IL should be set to 0.");
        this.throwIllegalStateExceptionIfStopped();
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("asynchPong");
        request.setArguments(new Object[]
        {new Long(serverTime)}, new Class[]
        {long.class});
        HTTPClientILStorageQueue.getInstance().put(request, this.clientIlId);
    }
    
    public void receive(ReceiveRequest[] messages) throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("receive(ReceiveRequest[] arraylength=" + String.valueOf(messages.length) + ")");
        }
        this.throwIllegalStateExceptionIfStopped();
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("asynchDeliver");
        request.setArguments(new Object[]
        {messages}, new Class[]
        {ReceiveRequest[].class});
        HTTPClientILStorageQueue.getInstance().put(request, this.clientIlId);
    }
    
    private void throwIllegalStateExceptionIfStopped() throws IllegalStateException
    {
        if (this.stopped)
        {
            throw new IllegalStateException("The client IL is stopped.");
        }
    }
    
}