/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.http;

import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;

import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;

import javax.jms.Destination;
import javax.jms.JMSException;
import javax.jms.Queue;
import javax.jms.TemporaryQueue;
import javax.jms.TemporaryTopic;
import javax.jms.Topic;

import org.jboss.logging.Logger;

import org.jboss.mq.AcknowledgementRequest;
import org.jboss.mq.ConnectionToken;
import org.jboss.mq.DurableSubscriptionID;
import org.jboss.mq.SpyDestination;
import org.jboss.mq.SpyMessage;
import org.jboss.mq.Subscription;
import org.jboss.mq.TransactionRequest;

import org.jboss.mq.il.ServerIL;

/**
 * Client proxy to the server.  For each request, an HTTP or HTTPS
 * request is created, and posted to the given URL.  The URL is supplied
 * in the HTTPServerILService MBean configuration, or automatically generated
 * by as localhost here, but can be overridden on the client side by specifying
 * a property name for the URL value, which will be resolved to a system property
 * on the client side.
 *
 * @author    Nathan Phelps (nathan@jboss.org)
 * @version   $Revision: 1.1.2.1 $
 * @created   January 15, 2003
 */
public class HTTPServerIL implements Serializable, ServerIL
{
    
    private static Logger log = Logger.getLogger(HTTPClient.class);
    
    private String serverUrlValue = null;
    private URL serverUrl = null;
    private String userNamePropertyKey = null;
    private String passwordPropertyKey = null;
    private String userName = null;
    private String password = null;
    
    public HTTPServerIL(String url) throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("created(String " + url + ")");
        }
        this.serverUrlValue = url;
    }
    
    public void acknowledge(ConnectionToken dc, AcknowledgementRequest item) throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("acknowledge(ConnectionToken " + dc.toString() + ", AcknowledgementRequest " + item.toString() + ")");
        }
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("acknowledge");
        request.setArguments(new Object[]
        {dc, item}, new Class[]
        {ConnectionToken.class, AcknowledgementRequest.class});
        this.postRequest(request);
    }
    
    public void addMessage(ConnectionToken dc, SpyMessage message) throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("addMessage(ConnectionToken " + dc.toString() + ", SpyMessage " + message.toString() + ")");
        }
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("addMessage");
        request.setArguments(new Object[]
        {dc, message}, new Class[]
        {ConnectionToken.class, SpyMessage.class});
        this.postRequest(request);
    }
    
    public String authenticate(String userName, String password) throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("authenticate(String " + userName + ", String " + password + ")");
        }
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("authenticate");
        request.setArguments(new Object[]
        {userName, password}, new Class[]
        {String.class, String.class});
        return (String)this.postRequest(request);
    }
    
    public SpyMessage[] browse(ConnectionToken dc, Destination dest, String selector) throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("browse(ConnectionToken " + dc.toString() + ", Destination " + dest.toString() + ", String " + selector + ")");
        }
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("browse");
        request.setArguments(new Object[]
        {dc, dest, selector}, new Class[]
        {ConnectionToken.class, Destination.class, String.class});
        return (SpyMessage[])this.postRequest(request);
    }
    
    public void checkID(String ID) throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("checkID(String " + ID + ")");
        }
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("checkID");
        request.setArguments(new Object[]
        {ID}, new Class[]
        {String.class});
        this.postRequest(request);
    }
    
    public String checkUser(String userName, String password) throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("checkUser(String " + userName + ", String " + password + ")");
        }
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("checkUser");
        request.setArguments(new Object[]
        {userName, password}, new Class[]
        {String.class, String.class});
        return (String)this.postRequest(request);
    }
    
    public ServerIL cloneServerIL() throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("cloneServerIL()");
        }
        return this;    // We can return this becuase we're stateless
    }
    
    public void connectionClosing(ConnectionToken dc) throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("connectionClosing(ConnectionToken " + dc.toString() + ")");
        }
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("connectionClosing");
        request.setArguments(new Object[]
        {dc}, new Class[]
        {ConnectionToken.class});
        this.postRequest(request);
    }
    
    public Queue createQueue(ConnectionToken dc, String dest) throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("createQueue(ConnectionToken " + dc.toString() + ", String " + dest.toString() + ")");
        }
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("createQueue");
        request.setArguments(new Object[]
        {dc, dest}, new Class[]
        {ConnectionToken.class, String.class});
        return (Queue)this.postRequest(request);
    }
    
    public Topic createTopic(ConnectionToken dc, String dest) throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("createTopic(ConnectionToken " + dc.toString() + ", String " + dest.toString() + ")");
        }
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("createTopic");
        request.setArguments(new Object[]
        {dc, dest}, new Class[]
        {ConnectionToken.class, String.class});
        return (Topic)this.postRequest(request);
    }
    
    public void deleteTemporaryDestination(ConnectionToken dc, SpyDestination dest) throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("deleteTemporaryDestination(ConnectionToken " + dc.toString() + ", SpyDestination " + dest.toString() + ")");
        }
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("deleteTemporaryDestination");
        request.setArguments(new Object[]
        {dc, dest}, new Class[]
        {ConnectionToken.class, SpyDestination.class});
        this.postRequest(request);
    }
    
    public void destroySubscription(ConnectionToken dc, DurableSubscriptionID id) throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("destroySubscription(ConnectionToken " + dc.toString() + ", DurableSubscriptionID " + id.toString() + ")");
        }
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("destroySubscription");
        request.setArguments(new Object[]
        {dc, id}, new Class[]
        {ConnectionToken.class, DurableSubscriptionID.class});
        this.postRequest(request);
    }
    
    public String getID() throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("getID()");
        }
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("getID");
        return (String)this.postRequest(request);
    }
    
    public TemporaryQueue getTemporaryQueue(ConnectionToken dc) throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("getTemporaryQueue(ConnectionToken " + dc.toString() + ")");
        }
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("getTemporaryQueue");
        request.setArguments(new Object[]
        {dc}, new Class[]
        {ConnectionToken.class});
        return (TemporaryQueue)this.postRequest(request);
    }
    
    public TemporaryTopic getTemporaryTopic(ConnectionToken dc) throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("getTemporaryTopic(ConnectionToken " + dc.toString() + ")");
        }
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("getTemporaryTopic");
        request.setArguments(new Object[]
        {dc}, new Class[]
        {ConnectionToken.class});
        return (TemporaryTopic)this.postRequest(request);
    }
    
    public void ping(ConnectionToken dc, long clientTime) throws Exception
    {
        // This is never called because we don't do pings.  It is here for completeness.
        if (log.isTraceEnabled())
        {
            log.trace("ping(ConnectionToken " + dc.toString() + ", long " + String.valueOf(clientTime) + ")");
        }
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("ping");
        request.setArguments(new Object[]
        {dc, new Long(clientTime)}, new Class[]
        {ConnectionToken.class, Long.class});
        this.postRequest(request);
    }
    
    public SpyMessage receive(ConnectionToken dc, int subscriberId, long wait) throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("receive(ConnectionToken " + dc.toString() + ", int " + String.valueOf(subscriberId) + ", long " + String.valueOf(wait) + ")");
        }
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("receive");
        request.setArguments(new Object[]
        {dc, new Integer(subscriberId), new Long(wait)}, new Class[]
        {ConnectionToken.class, Integer.class, Long.class});
        return (SpyMessage)this.postRequest(request);
    }
    
    public void setConnectionToken(ConnectionToken newConnectionToken) throws Exception
    {
        // Since we're stateless, we don't cache the ConnectionToken.
    }
    
    public void setEnabled(ConnectionToken dc, boolean enabled) throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("setEnabled(ConnectionToken " + dc.toString() + ", boolean " + String.valueOf(enabled) + ")");
        }
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("setEnabled");
        request.setArguments(new Object[]
        {dc, new Boolean(enabled)}, new Class[]
        {ConnectionToken.class, Boolean.class});
        this.postRequest(request);
    }
    
    public void subscribe(ConnectionToken dc, Subscription s) throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("subscribe(ConnectionToken " + dc.toString() + ", Subscription " + s.toString() + ")");
        }
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("subscribe");
        request.setArguments(new Object[]
        {dc, s}, new Class[]
        {ConnectionToken.class, Subscription.class});
        this.postRequest(request);
    }
    
    public void transact(ConnectionToken dc, TransactionRequest t) throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("transact(ConnectionToken " + dc.toString() + ", TransactionRequest " + t.toString() + ")");
        }
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("transact");
        request.setArguments(new Object[]
        {dc, t}, new Class[]
        {ConnectionToken.class, TransactionRequest.class});
        this.postRequest(request);
    }
    
    public void unsubscribe(ConnectionToken dc, int subscriptionId) throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("unsubscribe(ConnectionToken " + dc.toString() + ", int " + String.valueOf(subscriptionId) + ")");
        }
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("unsubscribe");
        request.setArguments(new Object[]
        {dc, new Integer(subscriptionId)}, new Class[]
        {ConnectionToken.class, Integer.class});
        this.postRequest(request);
    }
    
    private Object postRequest(HTTPILRequest request) throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("postRequest(HTTPILRequest " + request.toString() + ")");
        }
        if (this.serverUrl == null)
        {
            this.serverUrl = HTTPClient.resolveServerUrl(this.serverUrlValue);
        }
        return HTTPClient.post(this.serverUrl, request);
    }
}