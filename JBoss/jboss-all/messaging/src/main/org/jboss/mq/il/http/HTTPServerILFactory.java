/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.http;

import java.util.Properties;

import javax.jms.JMSException;

import org.jboss.logging.Logger;

import org.jboss.mq.il.ServerIL;
import org.jboss.mq.il.ServerILFactory;

/**
 * The HTTP/S implementation of the ServerILFactory object.
 *
 * @author    Nathan Phelps (nathan@jboss.org)
 * @version   $Revision: 1.1.2.1 $
 * @created   January 15, 2003
 */
public class HTTPServerILFactory implements ServerILFactory
{
    
    public static final String SERVER_IL_FACTORY = HTTPServerILFactory.class.getName();
    public static final String CLIENT_IL_SERVICE = HTTPClientILService.class.getName();
    public static final String SERVER_URL_KEY = "org.jboss.mq.il.http.url";
    public static final String TIMEOUT_KEY = "org.jboss.mq.il.http.timeout";
    public static final String REST_INTERVAL_KEY = "org.jboss.mq.il.http.restinterval";
    
    private static Logger log = Logger.getLogger(HTTPServerILFactory.class);
    
    private HTTPServerIL serverIL;
    
    public HTTPServerILFactory()
    {
        if(log.isTraceEnabled())
        {
            log.trace("created");
        }
    }
    
    public ServerIL getServerIL() throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("getServerIL()");
        }
        return this.serverIL;
    }
    
    public void init(Properties props) throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("init(Properties " + props.toString() + ")");
        }
        if (!props.containsKey(SERVER_URL_KEY))
        {
            if (log.isDebugEnabled())
            {
                log.debug("The supplied properties don't include a server URL entry.  Now checking to see if it is specified in the system properties.");
            }
            if (System.getProperties().containsKey(SERVER_URL_KEY))
            {
                if (log.isDebugEnabled())
                {
                    log.debug("The server URL property was found in the system properties.  Will use it.");
                }
                props.setProperty(SERVER_URL_KEY, System.getProperty(SERVER_URL_KEY));
            }
            else
            {
                throw new JMSException("A required connection property was not set: " + SERVER_URL_KEY);
            }
        }
        this.serverIL = new HTTPServerIL(props.getProperty(SERVER_URL_KEY));
        
    }
    
}