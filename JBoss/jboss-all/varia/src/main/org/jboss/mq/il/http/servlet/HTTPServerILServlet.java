/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.http.servlet;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;

import javax.naming.InitialContext;

import javax.servlet.ServletConfig;
import javax.servlet.ServletException;

import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import javax.management.MBeanServer;
import javax.management.AttributeNotFoundException;
import javax.management.ObjectName;
import javax.management.MalformedObjectNameException;
import javax.management.MBeanException;
import javax.management.InstanceNotFoundException;
import javax.management.ReflectionException;

import org.jboss.logging.Logger;

import org.jboss.mq.ConnectionToken;
import org.jboss.mq.il.http.HTTPILRequest;
import org.jboss.mq.il.http.HTTPILResponse;
import org.jboss.mq.il.http.HTTPClientIL;
import org.jboss.mq.il.http.HTTPClientILStorageQueue;

import org.jboss.mx.util.MBeanServerLocator;
import org.jboss.mq.server.JMSServerInvoker;
import org.jboss.mq.SpyMessage;

/**
 * This Servlet acts as a proxy to the JMS Server for the HTTP IL clients.
 * It receives posts from the HTTPServerIL in the form of HTTPILRequests,
 * invokes the approprate method on the server, and returns a HTTPILResponse.
 *
 * This also acts as a delegate for the HTTPClientILService, by receiving
 * POSTS, looking in HTTPILStorageQueue, retrieving HTTPRequests bound for the
 * client, packaging them in a HTTPILResponse.  These requests are generally
 * long lived to simulate asynch messaging.  When the HTTPClientILService POSTS
 * a HTTPILRequest the payload of the request includes timeout value which represents
 * the max time the request will last, before returning to the client.
 *
 * @author    Nathan Phelps (nathan@jboss.org)
 * @version   $Revision: 1.1.2.3 $
 * @created   January 15, 2003
 */
public class HTTPServerILServlet extends HttpServlet
{
    
    private static final String RESPONSE_CONTENT_TYPE = "application/x-java-serialized-object; class=org.jboss.mq.il.http.HTTPILResponse";
    
    private static Logger log = Logger.getLogger(HTTPServerILServlet.class);
    private MBeanServer server;
    private JMSServerInvoker invoker;
    
    public void init(ServletConfig config) throws ServletException
    {
        super.init(config);
        if (log.isTraceEnabled())
        {
            log.trace("init(ServletConfig " + config.toString() + ")");
        }
        this.server = MBeanServerLocator.locateJBoss();
        if (this.server == null)
        {
            throw new ServletException("Failed to locate the MBeanServer");
        }
        String invokerName = config.getInitParameter("Invoker");
        if (invokerName == null)
        {
            throw new ServletException("Invoker must be specified as servlet init parameter");
        }
        if (log.isDebugEnabled())
        {
            log.debug("Invoker set to '" + invokerName + ".'");
        }
        try
        {
            this.invoker = (JMSServerInvoker)server.getAttribute(new ObjectName(invokerName), "Invoker");
        }
        catch (Exception exception)
        {
            throw new ServletException("Failed to locate the JBossMQ invoker.");
        }
    }
    
    public void destroy()
    {
        if (log.isTraceEnabled())
        {
            log.trace("destroy()");
        }
    }
    
    protected void processRequest(HttpServletRequest request, HttpServletResponse response) throws ServletException, IOException
    {
        if (log.isTraceEnabled())
        {
            log.trace("processRequest(HttpServletRequest " + request.toString() + ", HttpServletResponse " + response.toString() + ")");
        }
        response.setContentType(RESPONSE_CONTENT_TYPE);
        ObjectOutputStream outputStream = new ObjectOutputStream(response.getOutputStream());
        try
        {
            ObjectInputStream inputStream = new ObjectInputStream(request.getInputStream());
            HTTPILRequest httpIlRequest = (HTTPILRequest)inputStream.readObject();
            String methodName = httpIlRequest.getMethodName();
            if (methodName.equals("clientListening"))
            {
                if (log.isTraceEnabled())
                {
                    log.trace("clientListening(HTTPILRequest " + httpIlRequest.toString() + ")");
                }
                String clientIlId = (String)httpIlRequest.getArguments()[0];
                long timeout = ((Long)httpIlRequest.getArguments()[1]).longValue();
                if (log.isDebugEnabled())
                {
                    log.debug("Listening on behalf of a ClientIL #" + clientIlId + " for " + String.valueOf(timeout) + " milliseconds.");
                }
                HTTPILRequest[] responseRequest = HTTPClientILStorageQueue.getInstance().get(clientIlId, timeout);
                if (log.isDebugEnabled())
                {
                    log.debug("The following lines reflect the HTTPILRequest object to be packaged and returned to ClientIL #" + clientIlId + " as an HTTPILResponse.");
                    for (int i = 0; i < responseRequest.length; i++)
                    {
                        log.debug("Response for ClientIL #" + clientIlId + " contains '" + responseRequest[i].toString() + ".'");
                    }
                }
                outputStream.writeObject(new HTTPILResponse(responseRequest));
            }
            else if (methodName.equals("getClientILIdentifer"))
            {
                if (log.isTraceEnabled())
                {
                    log.trace("getClientILIdentifer(HTTPILRequest " + httpIlRequest.toString() + ")");
                }
                String id = HTTPClientILStorageQueue.getInstance().getID();
                if (log.isDebugEnabled())
                {
                    log.debug("Provided ClientIL Id #" + id + ".");
                }
                outputStream.writeObject(new HTTPILResponse(id));
            }
            else if (methodName.equals("stopClientListening"))
            {
                if (log.isTraceEnabled())
                {
                    log.trace("stopClientListening(HTTPILRequest " + httpIlRequest.toString() + ")");
                }
                String clientIlId = (String)httpIlRequest.getArguments()[0];
                if (clientIlId != null)
                {
                    HTTPClientILStorageQueue.getInstance().purgeEntry(clientIlId);
                }
                outputStream.writeObject(new HTTPILResponse("Storage queue was purged."));
            }
            /* The following four 'else ifs' are special ServerIL cases where we can't
             * simply pass the invocation through due to the way the primitives
             * must be stored as objects.  In JDK 1.4, you can specify the primitive
             * class (i.e. Long.TYPE or long.class) and it works, however, this
             * is NOT the case with JDK 1.3.  Therefore, in order to support both
             * we've got to pass the primitive class type in its object form 
             * (i.e. Long.class), and then--in cases where they target object
             * expects a primitive--handle the unboxing manually.
             */
            else if (methodName.equals("ping"))
            {
                if (log.isTraceEnabled())
                {
                    log.trace("ping(HTTPILRequest " + httpIlRequest.toString() + ")");
                }
                ConnectionToken connectionToken = (ConnectionToken)httpIlRequest.getArguments()[0];
                long clientTime = ((Long)httpIlRequest.getArguments()[1]).longValue();
                this.invoker.ping(connectionToken, clientTime);
                outputStream.writeObject(new HTTPILResponse());
            }
            else if (methodName.equals("receive"))
            {
                if (log.isTraceEnabled())
                {
                    log.trace("receive(HTTPILRequest " + httpIlRequest.toString() + ")");
                }
                ConnectionToken connectionToken = (ConnectionToken)httpIlRequest.getArguments()[0];
                int subscriberId = ((Integer)httpIlRequest.getArguments()[1]).intValue();
                long wait = ((Long)httpIlRequest.getArguments()[2]).longValue();
                SpyMessage message = this.invoker.receive(connectionToken, subscriberId, wait);
                outputStream.writeObject(new HTTPILResponse(message));
                if (message != null && log.isDebugEnabled())
                {
                    log.debug("Returned an instance of '" + message.getClass().toString() + "' with value of '" + message.toString() + "'");
                }
            }
            else if (methodName.equals("setEnabled"))
            {
                if (log.isTraceEnabled())
                {
                    log.trace("setEnabled(HTTPILRequest " + httpIlRequest.toString() + ")");
                }
                ConnectionToken connectionToken = (ConnectionToken)httpIlRequest.getArguments()[0];
                boolean enabled = ((Boolean)httpIlRequest.getArguments()[1]).booleanValue();
                this.invoker.setEnabled(connectionToken, enabled);
                outputStream.writeObject(new HTTPILResponse());
            }
            else if (methodName.equals("unsubscribe"))
            {
                if (log.isTraceEnabled())
                {
                    log.trace("unsubscribe(HTTPILRequest " + httpIlRequest.toString() + ")");
                }
                ConnectionToken connectionToken = (ConnectionToken)httpIlRequest.getArguments()[0];
                int subscriberId = ((Integer)httpIlRequest.getArguments()[1]).intValue();
                this.invoker.unsubscribe(connectionToken, subscriberId);
                outputStream.writeObject(new HTTPILResponse());
            }
            else
            {
                if (log.isTraceEnabled())
                {
                    log.trace("HTTPILRequest recieved: " + httpIlRequest.toString());
                }
                Method method = this.invoker.getClass().getMethod(methodName, httpIlRequest.getArgumentTypes());
                Object returnValue = method.invoke(this.invoker, httpIlRequest.getArguments());
                if (log.isDebugEnabled())
                {
                    log.debug("Invoked method '" + methodName + ".'");
                }
                outputStream.writeObject(new HTTPILResponse(returnValue));
                if (returnValue != null && log.isDebugEnabled())
                {
                    log.debug("Returned an instance of '" + returnValue.getClass().toString() + "' with value of '" + returnValue.toString() + "'");
                }
            }
        }
        catch (InvocationTargetException invocatonTargetException)
        {
            Throwable targetException = invocatonTargetException.getTargetException();
            if (log.isDebugEnabled())
            {
                log.debug("The underlying invoker (i.e. The JMS Server itself) threw in exception of type '" + targetException.getClass().getName() + "' and a message of '" + targetException.getMessage() + ".'  This exception is being propogated to the client as a  HTTPILResponse.");
            }
            outputStream.writeObject(new HTTPILResponse(targetException));
        }
        catch (Exception exception)
        {
            if (log.isDebugEnabled())
            {
                log.debug("Threw an exception of type '" + exception.getClass().getName() + "' with a message of '" + exception.getMessage() + ".'  This exception is being propogated to the client as a HTTPILResponse.");
            }
            outputStream.writeObject(new HTTPILResponse(exception));
        }
        outputStream.close();
    }
    
    protected void doGet(HttpServletRequest request, HttpServletResponse response) throws ServletException, IOException
    {
        if (log.isTraceEnabled())
        {
            log.trace("doGet(HttpServletRequest " + request.toString() + ", HttpServletResponse " + response.toString() + ")");
        }
        response.getWriter().print("<html><head><title>JBossMQ HTTP-IL Servlet</title><head><body><h1>This is the JBossMQ HTTP-IL</h1></body></html>");
    }
    
    protected void doPost(HttpServletRequest request, HttpServletResponse response) throws ServletException, IOException
    {
        if (log.isTraceEnabled())
        {
            log.trace("doPost() defers to processRequest, see the parameters in its trace.");
        }
        processRequest(request, response);
    }
    
    public String getServletInfo()
    {
        return "Provides an HTTP/S interface to JBossMQ";
    }
}