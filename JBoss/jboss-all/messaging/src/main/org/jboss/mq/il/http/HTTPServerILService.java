/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.http;

import java.net.InetAddress;
import java.net.URL;

import java.util.Properties;

import org.jboss.mq.il.ServerIL;
import org.jboss.mq.il.ServerILFactory;
import org.jboss.mq.il.ServerILJMXService;

/**
 * Implements the ServerILJMXService which is used to manage the HTTP/S IL.
 *
 * @author    Nathan Phelps (nathan@jboss.org)
 * @version   $Revision: 1.1.2.2 $
 * @created   January 15, 2003
 *
 * @jmx:mbean extends="org.jboss.mq.il.ServerILJMXServiceMBean"
 */
public class HTTPServerILService extends ServerILJMXService implements HTTPServerILServiceMBean
{
    
    private HTTPServerIL serverIL;
    private String url = null;
    private String urlPrefix = "http://";
    private int urlPort = 8080;
    private String urlSuffix = "jbossmq-httpil/HTTPServerILServlet";
    private String urlHostName = null;
    private boolean useHostName = false;
    private long timeout = 60 * 1000;
    private long restInterval = 0;
    
    public HTTPServerILService()
    {
    }
    
    public String getName()
    {
        return "JBossMQ-HTTPServerIL";
    }
    
    public ServerIL getServerIL()
    {
        return this.serverIL;
    }
    
    public Properties getClientConnectionProperties()
    {
        Properties properties = super.getClientConnectionProperties();
        properties.setProperty(HTTPServerILFactory.CLIENT_IL_SERVICE_KEY, HTTPServerILFactory.CLIENT_IL_SERVICE);
        properties.setProperty(HTTPServerILFactory.SERVER_URL_KEY, this.url);
        properties.setProperty(HTTPServerILFactory.TIMEOUT_KEY, String.valueOf(this.timeout));
        properties.setProperty(HTTPServerILFactory.REST_INTERVAL_KEY, String.valueOf(this.restInterval));
        return properties;
    }
    
    public void startService() throws Exception
    {
        super.startService();
        this.pingPeriod = 0;    // We don't do pings.
        if (this.url == null)
        {
            this.url = this.getConstructedURL();
        }
        this.serverIL = new HTTPServerIL(this.url);
        super.bindJNDIReferences();
    }
    
    public void stopService()
    {
        try
        {
            unbindJNDIReferences();
        }
        catch (Exception e)
        {
            e.printStackTrace();
        }
    }
    
    /**
     * Set the HTTPIL default timeout--the number of seconds that the ClientILService
     * HTTP requests will wait for messages.  This can be overridden on the client
     * by setting the system property org.jboss.mq.il.http.timeout to an int value
     * of the number of seconds.  NOTE: This value should be in seconds, not millis.
     *
     * @jmx:managed-attribute
     */
    public void setTimeOut(int timeout)
    {
        this.timeout = timeout * 1000;  // provided in seconds so turn it into Millis
    }
    
    /**
     * Get the HTTPIL default timeout
     *
     * @jmx:managed-attribute
     */
    public int getTimeOut()
    {
        return (int)this.timeout / 1000; // stored in Millis, but return it in seconds
    }
    
    /**
     * Set the HTTPIL default RestInterval--the number of seconds the ClientILService
     * will sleep after each client request.  The default is 0, but you can set this
     * value in conjunction with the TimeOut value to implement a pure timed based
     * polling mechanism.  For example, you could simply do a short lived request by
     * setting the TimeOut value to 0 and then setting the RestInterval to 60.  This
     * would cause the ClientILService to send a single non-blocking request to the
     * server, return any messages if available, then sleep for 60 seconds, before
     * issuing another request.  Like the TimeOut value, this can be explicitly
     * overriden on a given client by specifying the org.jboss.mq.il.http.restinterval
     * with the number of seconds you wish to wait between requests.
     *
     * @jmx:managed-attribute
     */
    public void setRestInterval(int restInterval)
    {
        this.restInterval = restInterval * 1000;  // provided in seconds so turn it into Millis
    }
    
    /**
     * Get the HTTPIL default RestInterval
     *
     * @jmx:managed-attribute
     */
    public int getRestInterval()
    {
        return (int)this.restInterval / 1000; // stored in Millis, but return it in seconds
    }
    
    /**
     * Set the HTTPIL URL.  This value takes precedence over any individual values
     * set (i.e. the URLPrefix, URLSuffix, URLPort, etc.)  It my be a actual
     * URL or a propertyname which will be used on the client side to resolve the
     * proper URL by calling System.getProperty(propertyname).
     *
     * @jmx:managed-attribute
     */
    public void setURL(String url)
    {
        this.url = url;
        // Set all specific url properties to null values.  I know we could parse
        // the URL and set these, but the url might be a property name.  Besides
        // letting them have value in this case might mislead people into thinking
        // that the value mattered.  As the Javadoc states, when the URL is set
        // all these value are irrelivant.
        this.urlPrefix = null;
        this.urlHostName = null;
        this.urlPort = 0;
        this.urlSuffix = null;
        this.useHostName = false;
    }
    
    /**
     * Get the HTTPIL URL.  This value takes precedence over any individual values
     * set (i.e. the URLPrefix, URLSuffix, URLPort, etc.)  It my be a actual
     * URL or a propertyname which will be used on the client side to resolve the
     * proper URL by calling System.getProperty(propertyname).
     *
     * @jmx:managed-attribute
     */
    public String getURL()
    {
        return this.url;
    }
    
    /**
     * Set the HTTPIL URLPrefix.  I.E. "http://" or "https://"
     * The default is "http://"
     *
     * @jmx:managed-attribute
     */
    public void setURLPrefix(String prefix)
    {
        this.urlPrefix = prefix;
    }
    
    /**
     * Get the HTTPIL URLPrefix.  I.E. "http://" or "https://"
     * The default is "http://"
     *
     * @jmx:managed-attribute
     */
    public String getURLPrefix()
    {
        return this.urlPrefix;
    }
    
    /**
     * Set the HTTPIL URLName.
     *
     * @jmx:managed-attribute
     */
    public void setURLHostName(String hostname)
    {
        this.urlHostName = hostname;
    }
    
    /**
     * Get the HTTPIL URLHostName.
     *
     * @jmx:managed-attribute
     */
    public String getURLHostName()
    {
        return this.urlHostName;
    }
    
    /**
     * Set the HTTPIL URLPort.
     * The default is 8080
     *
     * @jmx:managed-attribute
     */
    public void setURLPort(int port)
    {
        this.urlPort = port;
    }
    
    /**
     * Get the HTTPIL URLPort.
     * The default is 8080
     *
     * @jmx:managed-attribute
     */
    public int getURLPort()
    {
        return this.urlPort;
    }
    
    /**
     * Set the HTTPIL URLSuffix.  I.E. The section of the URL after the port
     * The default is "jbossmq-httpil/HTTPServerILServlet"
     *
     * @jmx:managed-attribute
     */
    public void setURLSuffix(String suffix)
    {
        this.urlSuffix = suffix;
    }
    
    /**
     * Get the HTTPIL URLSuffix.  I.E. The section of the URL after the port
     * The default is "jbossmq-httpil/HTTPServerILServlet"
     *
     * @jmx:managed-attribute
     */
    public String getURLSuffix()
    {
        return this.urlSuffix;
    }
    
    
    /**
     * Set the HTTPIL UseHostName flag.
     * if true the default URL will include a hostname, if false it will include
     * an IP adddress.  The default is false
     *
     * @jmx:managed-attribute
     */
    public void setUseHostName(boolean value)
    {
        this.useHostName = value;
    }
    
    /**
     * Get the HTTPIL UseHostName flag.
     * if true the default URL will include a hostname, if false it will include
     * an IP adddress.  The default is false
     *
     * @jmx:managed-attribute
     */
    public boolean getUseHostName()
    {
        return this.useHostName;
    }
    
    
    private String getConstructedURL() throws Exception
    {
        if (System.getProperty(HTTPServerILFactory.SERVER_URL_KEY) != null)
        {
            return System.getProperty(HTTPServerILFactory.SERVER_URL_KEY);
        }
        else
        {
            String hostName = null;
            if (this.urlHostName == null)
            {
                if (this.useHostName)
                {
                    hostName = InetAddress.getLocalHost().getHostName();
                }
                else
                {
                    hostName = InetAddress.getLocalHost().getHostAddress();
                }
            }
            else
            {
                hostName = this.urlHostName;
            }
            return this.urlPrefix + hostName + ":" + String.valueOf(this.urlPort) + "/" + this.urlSuffix;
        }
    }
}