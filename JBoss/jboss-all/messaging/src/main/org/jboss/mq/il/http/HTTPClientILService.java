/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.http;

import java.lang.reflect.Method;

import java.net.URL;

import java.util.Properties;

import org.jboss.logging.Logger;

import org.jboss.mq.Connection;
import org.jboss.mq.ConnectionToken;

import org.jboss.mq.il.ClientIL;
import org.jboss.mq.il.ClientILService;

/**
 * The HTTP/S implementation of the ClientILService object.  One of these
 * exists for each Connection object.  Normally, this would be where you
 * would boot up a client-side listener for the server to invoke.  However
 * in this case, we poll the server instead.  In short, the ClientIL that we
 * provide, once serialized and shipped to the server, simply places requests
 * the server makes to it in a server-side storage mechanism.  Then, when we
 * send an HTTP request to the server, the servlet looks for queued requests
 * waiting for us, batches them up and returns them in the response.  Since
 * we place ALL requests delivered to ANY instance of the ClientIL in a
 * central storage queue, we have to have a way to get only the requests placed
 * in storage by OUR ClientIL.  Originally, I attempted to use the ConnectionId
 * for this purpose, but it proooved to be less than ideal due to the fact that
 * it created many cases where requests were being fielded to an instance of a
 * ClientIL which was sent over the wire prior to the server returning our
 * ConnectionId.  This resulted in lost requests.  Furthermore, since this had
 * no control over exactly when the ConnectionId was set, we were forced to
 * loop until it was not null.  The current implementation doesn’t' suffer from
 * these issues, as we can take full control over the process of setting our
 * identifier and therefore, set the identifier on our ClientIL at creation time.
 *
 * @author    Nathan Phelps (nathan@jboss.org)
 * @version   $Revision: 1.1.2.1 $
 * @created   January 15, 2003
 */
public class HTTPClientILService implements Runnable, ClientILService
{
    
    private static Logger log = Logger.getLogger(HTTPClientILService.class);
    
    private HTTPClientIL clientIL;
    private Connection connection;
    private URL url = null;
    private long timeout = 60000;
    private long restInterval = 0;
    private Thread worker;
    private String clientILIdentifier;
    
    private static int threadNumber= 0;
    
    public HTTPClientILService()
    {
        if (log.isTraceEnabled())
        {
            log.trace("created");
        }
    }
    
    public ClientIL getClientIL() throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("getClientIL()");
        }
        return this.clientIL;
    }
    
    public void init(Connection connection, Properties props) throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("init(Connection " + connection.toString() + ", Properties " + props.toString() + ")");
        }
        this.connection = connection;
        this.url = HTTPClient.resolveServerUrl(props.getProperty(HTTPServerILFactory.SERVER_URL_KEY));
        this.clientILIdentifier = this.getClientILIdentifier(this.url);
        this.clientIL = new HTTPClientIL(this.clientILIdentifier);
        try
        {
            if (System.getProperties().containsKey(HTTPServerILFactory.TIMEOUT_KEY))
            {
                this.timeout = Long.valueOf(System.getProperty(HTTPServerILFactory.TIMEOUT_KEY)).longValue();
            }
            else
            {
                this.timeout = Long.valueOf(props.getProperty(HTTPServerILFactory.TIMEOUT_KEY)).longValue();
            }
            if (System.getProperties().containsKey(HTTPServerILFactory.REST_INTERVAL_KEY))
            {
                this.restInterval = Long.valueOf(System.getProperty(HTTPServerILFactory.REST_INTERVAL_KEY)).longValue();
            }
            else
            {
                this.restInterval = Long.valueOf(props.getProperty(HTTPServerILFactory.REST_INTERVAL_KEY)).longValue();
            }
        }
        catch (Exception exception)
        {} // we'll just use the default value
    }
    
    public void start() throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("start()");
        }
        clientIL.stopped = false;
        worker = new Thread(this.connection.threadGroup, this, "HTTPClientILService-" + threadNumber++);
        worker.setDaemon(true);
        worker.start();
    }
    
    public void stop() throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("stop()");
        }
        clientIL.stopped = true;
    }
    
    public void run()
    {
        if (log.isTraceEnabled())
        {
            log.trace("run()");
        }
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("clientListening");
        while (clientIL.stopped == false)
        {
            try
            {
                if (this.clientILIdentifier != null && clientIL.stopped == false)
                {
                    request.setArguments(new Object[]
                    {this.clientILIdentifier, new Long(this.timeout)}, new Class[]
                    {String.class, Long.class});
                    if (log.isDebugEnabled())
                    {
                        log.debug("Sending a request to '" + this.url.toString() + "' for ClientIL #" + this.clientILIdentifier + ".");
                    }
                    // The server responds with a HTTPILRequest object, not a HTTPILResponse object as you might expect.
                    // this is becuase the server is invoking the client.
                    HTTPILRequest[] response = (HTTPILRequest[])HTTPClient.post(this.url, request);
                    if (response != null)
                    {
                        if (log.isDebugEnabled())
                        {
                            log.debug("Logging each response received in this batch for ClientIL #" + this.clientILIdentifier + ".");
                        }
                        for (int i = 0; i < response.length; i++)
                        {
                            if (log.isDebugEnabled())
                            {
                                log.debug(response.toString());
                            }
                            Method method = this.connection.getClass().getMethod(response[i].getMethodName(), response[i].getArgumentTypes());
                            method.invoke(this.connection, response[i].getArguments());
                            if (log.isDebugEnabled())
                            {
                                log.debug("Server invoked method '" + method.getName() + "' on ClientIL #" + this.clientILIdentifier + ".");
                            }
                        }
                    }
                    else
                    {
                        log.warn("The request posted to '" + this.url.toString() + "' on behalf of ClientIL #" + this.clientILIdentifier + " returned an unexpected response.");
                    }
                    
                    try
                    {
                        if (log.isDebugEnabled())
                        {
                            log.debug("Resting " + String.valueOf(this.restInterval) + " milliseconds on ClientIL #" + this.clientILIdentifier + ".");
                        }
                        this.worker.sleep(this.restInterval);
                    }
                    catch (InterruptedException exception)
                    {}   // We'll just skip the rest, and go ahead and issue another request immediatly.
                    
                }
                else
                {
                    log.warn("ClientIL Id is null, waiting 50 milliseconds to get one.");
                    this.worker.sleep(50);
                }
            }
            catch (Exception exception)
            {
                if (log.isDebugEnabled())
                {
                    log.debug("Exception of type '" + exception.getClass().getName() + "' occured when trying to receive request from server URL '" + this.url + ".'");
                }
                this.connection.asynchFailure(exception.getMessage(), exception);
                break;
            }
        }
        if (this.clientIL.stopped)
        {
            if (log.isDebugEnabled())
            {
                log.debug("Notifying the server that ClientIL #" + this.clientILIdentifier + " has stopped.");
            }
            try
            {
                HTTPILRequest stopRequest = new HTTPILRequest();
                request.setMethodName("stopClientListening");
                request.setArguments(new Object[]
                {this.clientILIdentifier}, new Class[]
                {String.class});
                HTTPClient.post(this.url, request);
            }
            catch (Exception exception)
            {
                if (log.isDebugEnabled())
                {
                    log.debug("Attempt to notify the server that ClientIL #" + this.clientILIdentifier + " failed due to exception with description '" + exception.getMessage() + ".' This means that requests will stay in the storage queue even though the client has stopped.");
                }
            }
        }
    }
    
    private String getClientILIdentifier(URL url) throws Exception
    {
        HTTPILRequest request = new HTTPILRequest();
        request.setMethodName("getClientILIdentifer");
        return (String)HTTPClient.post(url, request);
    }
}