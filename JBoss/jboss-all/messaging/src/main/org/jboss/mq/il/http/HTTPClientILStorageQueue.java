/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.http;

import java.util.HashMap;
import java.util.Map;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;

import EDU.oswego.cs.dl.util.concurrent.LinkedQueue;    // I *HATE* the capital EDU!  Drives me NUTS!

import org.jboss.logging.Logger;

/**
 * Stores requests on behalf of clients.  This could of course, be done
 * with a JMS queue, but I decided this would be a lighter weight solution.
 *
 * @author    Nathan Phelps (nathan@jboss.org)
 * @version   $Revision: 1.1.2.4 $
 * @created   January 15, 2003
 */
public class HTTPClientILStorageQueue
{
    
    private static Logger log = Logger.getLogger(HTTPClientILStorageQueue.class);
    private static HTTPClientILStorageQueue instance = null;
    private Map map = new HashMap();
    private Object queueLock = new Object();
    
    /* The static Id variable provides a mechanism for us to identify a particular
     * ClientIL's requests in the map.  This value is retrieved by the ClientILService
     * init method, which sends an HTTPIL request to the servlet, which in turn
     * asks this singleton instance for the next Id.  If you are wondering why
     * we need to do this, why we just can't use the *actual* connection Id,
     * please see my comments in ClientILService.
     */
    private static long id = 100;  //Start at 100 so we don't get confused with the Connection Ids.
    private static Object idLock = new Object();
    
    private HTTPClientILStorageQueue()
    {
        if (log.isTraceEnabled())
        {
            log.trace("created");
        }
    }
    
    public static synchronized HTTPClientILStorageQueue getInstance()
    {
        if (log.isTraceEnabled())
        {
            log.trace("getInstance()");
        }
        if (instance == null)
        {
            instance = new HTTPClientILStorageQueue();
        }
        return instance;
    }
    
    public void put(HTTPILRequest request, String clientIlId) throws InterruptedException
    {
        if (log.isTraceEnabled())
        {
            log.trace("put(HTTPILRequest " + request.toString() + ", String " + clientIlId + ")");
        }
        if (clientIlId == null)
        {
            log.warn("A request was put in a storage queue for a null ClientIl.");
            
            return;
        }
        synchronized(this.queueLock)
        {
            if (this.map.containsKey(clientIlId))
            {
                if (log.isDebugEnabled())
                {
                    log.debug("ClientIL #" + clientIlId + " has existing storage queue, adding request to it.");
                }
                LinkedQueue queue = (LinkedQueue)this.map.get(clientIlId);
                queue.put(request);
            }
            else
            {
                if (log.isDebugEnabled())
                {
                    log.debug("ClientIL #" + clientIlId + " doesn't have a storage queue.  Creating one and adding the request.");
                }
                LinkedQueue queue = new LinkedQueue();
                queue.put(request);
                this.map.put(clientIlId, queue);
            }
        }
    }
    
    public HTTPILRequest[] get(String clientIlId, long timeout)
    {
        if (log.isTraceEnabled())
        {
            log.trace("get(String " + clientIlId + ")");
        }
        
        if (clientIlId == null)
        {
            log.warn("A get was issued with a null clientIL Id.");
        }
        
        LinkedQueue queue;
        synchronized(queueLock)
        {
            queue = (LinkedQueue)this.map.get(clientIlId);
            if (queue == null)
            {
                if (log.isDebugEnabled())
                {
                    log.debug("ClientIL #" + clientIlId + " doesn't have a storage queue.  Creating new one.");
                }
                queue = new LinkedQueue();
                this.map.put(clientIlId, queue);  // create a new queue for this client
            }
        }
        ArrayList messageList = new ArrayList();
        try
        {
            if (log.isDebugEnabled())
            {
                log.debug("Polling the queue for " + String.valueOf(timeout) + " milliseconds on behalf of clientIL #" + clientIlId + ".");
            }
            Object object = queue.poll(timeout);
            if (object != null)
            {
                if (log.isDebugEnabled())
                {
                    log.debug("Poll returned a HTTPILRequest, adding it to our list of requests to deliver to clientIL #" + clientIlId + ".");
                }
                messageList.add(object);
                while ((object = queue.poll(0)) != null)
                {
                    if (log.isDebugEnabled())
                    {
                        log.debug("We had a request, so we're are going to see if there are any more for us, but we're not going to block this time.");
                    }
                    messageList.add(object);
                    if (log.isDebugEnabled())
                    {
                        log.debug("Added request.");
                    }
                }
            }
        }
        catch (InterruptedException exception)
        {
            if (log.isDebugEnabled())
            {
                log.debug("An interruptedException was triggered.  We'll just deliver what we have to the client and try again next time.");
            }
        }
        finally
        {
            if (log.isDebugEnabled())
            {
                log.debug("Returning " + String.valueOf(messageList.size()) + " requests to clientIL #" + clientIlId + ".");
            }
            return this.createArrayFromList(messageList);   // this could be empty, and if so, its OK.
        }
    }
    
    public void purgeEntry(String clientIlId)
    {
        if (log.isTraceEnabled())
        {
            log.trace("purgeEntry(String " + clientIlId + ")");
        }
        Object entry;
        synchronized(this.queueLock)
        {
            entry = this.map.remove(clientIlId);
        }
        if (entry != null && log.isDebugEnabled())
        {
            log.debug("Purged storage queue entry for ClientIL #" + clientIlId + ".");
        }
        
    }
    
    public String getID()
    {
        if (log.isTraceEnabled())
        {
            log.trace("getID()");
        }
        synchronized(idLock)
        {
           return String.valueOf(++id);
        }
    }
    
    private HTTPILRequest[] createArrayFromList(ArrayList list)
    {
        if (log.isTraceEnabled())
        {
            log.trace("createArrayFromList(ArrayList length=" + String.valueOf(list.size()) + ")");
        }
        HTTPILRequest[] requests = new HTTPILRequest[list.size()];
        Iterator itemList = list.iterator();
        int i = 0;
        while (itemList.hasNext())
        {
            requests[i] = (HTTPILRequest)itemList.next();
            i++;
        }
        return requests;
    }
}
