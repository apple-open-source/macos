/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.loadbalancer;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.Set;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.servlet.ServletException;
import javax.servlet.http.Cookie;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.apache.commons.httpclient.Header;
import org.apache.commons.httpclient.HttpClient;
import org.apache.commons.httpclient.HttpMethod;
import org.apache.commons.httpclient.HttpState;
import org.apache.commons.httpclient.MultiThreadedHttpConnectionManager;
import org.apache.commons.httpclient.cookie.CookiePolicy;
import org.apache.commons.httpclient.methods.DeleteMethod;
import org.apache.commons.httpclient.methods.GetMethod;
import org.apache.commons.httpclient.methods.HeadMethod;
import org.apache.commons.httpclient.methods.OptionsMethod;
import org.apache.commons.httpclient.methods.PostMethod;
import org.apache.commons.httpclient.methods.PutMethod;
import org.jboss.logging.Logger;
import org.jboss.mx.util.MBeanServerLocator;
import org.jboss.web.loadbalancer.scheduler.NoHostAvailableException;
import org.jboss.web.loadbalancer.scheduler.Scheduler;
import org.jboss.web.loadbalancer.util.Constants;
import org.jboss.metadata.MetaData;

import org.w3c.dom.Element;

/**
 * The Loadbalancer core class.
 *
 * @jmx:mbean name="jboss.web.loadbalancer: service=Loadbalancer"
 *
 * @author Thomas Peuss <jboss@peuss.de>
 * @version $Revision: 1.3.2.2 $
 */
public class Loadbalancer
    implements LoadbalancerMBean
{
  protected static Logger log = Logger.getLogger(Loadbalancer.class);

  // The connection manager
  protected MultiThreadedHttpConnectionManager connectionManager;

  // request header elements that must not be copied to client
  protected static Set ignorableHeader = new HashSet();

  protected int connectionTimeout = 20000;

  protected Scheduler scheduler;
  protected ObjectName schedulerName;

  protected static final int MAX_RETRIES = 5;

  static
  {
    // this header elements are not copied from
    // the HttpClient response to the request client.
    ignorableHeader.add("content-length");
    ignorableHeader.add("server");
    ignorableHeader.add("transfer-encoding");
    ignorableHeader.add("cookie");
    ignorableHeader.add("set-cookie");
    ignorableHeader.add("host");
  }

  protected Loadbalancer(Element config) throws ServletException
  {
    try
    {
      schedulerName=new ObjectName(MetaData.getUniqueChildContent(config,"scheduler-name"));

      String schedulerClassName = MetaData.getUniqueChildContent(config,
          "scheduler-class-name");

      try
      {
        scheduler = (Scheduler) Thread.currentThread().getContextClassLoader().
            loadClass(
            schedulerClassName).newInstance();
        scheduler.init(config);
      }
      catch (Exception e)
      {
        log.error("Could not create scheduler", e);
        throw new ServletException(e);
      }
      try
      {
        connectionTimeout = Integer.parseInt(
            MetaData.getUniqueChildContent(config, "connection-timeout"));

        log.debug("Setting connection timeout to " + connectionTimeout);
      }
      catch (NumberFormatException ex)
      {
        log.warn(
            "Cannot read connection timeout value. Using default connection timeout");
        //ignore -> use default
      }
    }
    catch (Exception ex)
    {
      log.error("Could not read config " + config, ex);
      throw new ServletException(ex);
    }
    connectionManager = new MultiThreadedHttpConnectionManager();

    // We disable this because the web-container limits the maximum connection count anyway
    connectionManager.setMaxConnectionsPerHost(Integer.MAX_VALUE);
    connectionManager.setMaxTotalConnections(Integer.MAX_VALUE);
  }

  protected HttpMethod createMethod(
      HttpServletRequest request,
      HttpServletResponse response,
      int requestMethod) throws NoHostAvailableException
  {
    String url = null;
    HttpMethod method = null;

    // get target host from scheduler
    url = scheduler.getHost(request, response).toExternalForm();

    String path = url.substring(0, url.length() - 1) + request.getRequestURI();

    switch (requestMethod)
    {
      case Constants.HTTP_METHOD_GET:
        method = new GetMethod(path);
        break;
      case Constants.HTTP_METHOD_POST:
        method = new PostMethod(path);
        break;
      case Constants.HTTP_METHOD_DELETE:
        method = new DeleteMethod(path);
        break;
      case Constants.HTTP_METHOD_HEAD:
        method = new HeadMethod(path);
        break;
      case Constants.HTTP_METHOD_OPTIONS:
        method = new OptionsMethod(path);
        break;
      case Constants.HTTP_METHOD_PUT:
        method = new PutMethod(path);
        break;
      default:
        throw new IllegalStateException("Unknown Request Method " +
                                        request.getMethod());
    }

    return method;
  }

  protected HttpMethod addRequestData(HttpServletRequest request,
                                      HttpMethod method)
  {
    // add GET-data to query string
    if (request.getQueryString() != null)
    {
      method.setQueryString(request.getQueryString());
    }

    // add POST-data to the request
    if (method instanceof PostMethod)
    {
      PostMethod postMethod = (PostMethod) method;

      Enumeration paramNames = request.getParameterNames();
      while (paramNames.hasMoreElements())
      {
        String paramName = (String) paramNames.nextElement();
        postMethod.addParameter(paramName, request.getParameter(paramName));
      }
    }

    return method;
  }

  protected HttpClient prepareServerRequest(
      HttpServletRequest request,
      HttpServletResponse response,
      HttpMethod method)
  {
    // clear state
    HttpClient client = new HttpClient(connectionManager);
    client.setStrictMode(false);
    client.setTimeout(connectionTimeout);
    method.setFollowRedirects(false);
    method.setDoAuthentication(false);
    client.getState().setCookiePolicy(CookiePolicy.COMPATIBILITY);

    Enumeration reqHeaders = request.getHeaderNames();

    while (reqHeaders.hasMoreElements())
    {
      String headerName = (String) reqHeaders.nextElement();
      String headerValue = request.getHeader(headerName);

      if (!ignorableHeader.contains(headerName.toLowerCase()))
      {
        method.setRequestHeader(headerName, headerValue);
      }
    }

    //Cookies
    Cookie[] cookies = request.getCookies();
    HttpState state = client.getState();

    for (int i = 0; cookies != null && i < cookies.length; ++i)
    {
      Cookie cookie = cookies[i];

      org.apache.commons.httpclient.Cookie reqCookie =
          new org.apache.commons.httpclient.Cookie();

      reqCookie.setName(cookie.getName());
      reqCookie.setValue(cookie.getValue());

      if (cookie.getPath() != null)
      {
        reqCookie.setPath(cookie.getPath());
      }
      else
      {
        reqCookie.setPath("/");
      }

      reqCookie.setSecure(cookie.getSecure());

      reqCookie.setDomain(method.getHostConfiguration().getHost());
      state.addCookie(reqCookie);
    }
    return client;
  }

  protected void handleRequest(
      HttpServletRequest request,
      HttpServletResponse response,
      HttpMethod method) throws ServletException, IOException
  {

    boolean reschedule = false;

    try
    {
      HttpClient client = prepareServerRequest(request, response, method);
      int tries = 0;
      while (tries < MAX_RETRIES)
      {
        try
        {
          client.executeMethod(method);
          break;
        }
        catch (IOException ex)
        {
          try
          {
            method.releaseConnection();
          }
          catch (Exception e)
          {
            //ignore
          }

          tries++;
           log.info("Connect retry no. " + tries);
           if (log.isDebugEnabled())
              log.debug(ex);

        }
      }
      if (tries < MAX_RETRIES)
      {
        parseServerResponse(request, response, client, method);
      }
      else
      {
        log.error("Max retries reached - giving up. Host will be marked down");

        // Inform Scheduler of node problems
        scheduler.setNodeDown(request, response, client, method);
        reschedule = true;
      }
    }
    finally
    {
      try
      {
        method.releaseConnection();
      }
      catch (Exception e)
      {
        //ignore
      }
    }
    // try again?
    if (reschedule)
    {
      String redirectURI = request.getRequestURI();

      if (request.getQueryString() != null)
      {
        redirectURI += "?" + request.getQueryString();
      }

      response.sendRedirect(redirectURI);
    }
  }

  protected void parseServerResponse(
      HttpServletRequest request,
      HttpServletResponse response,
      HttpClient client,
      HttpMethod method) throws ServletException, IOException
  {
    response.setStatus(method.getStatusCode());

    //Cookies
    org.apache.commons.httpclient.Cookie[] respCookies =
        client.getState().getCookies();

    for (int i = 0; i < respCookies.length; ++i)
    {
      Cookie cookie =
          new Cookie(respCookies[i].getName(), respCookies[i].getValue());

      if (respCookies[i].getPath() != null)
      {
        cookie.setPath(respCookies[i].getPath());
      }
      response.addCookie(cookie);
    }

    Header[] header = method.getResponseHeaders();

    for (int i = 0; i < header.length; ++i)
    {
      if (!ignorableHeader.contains(header[i].getName().toLowerCase()))
      {
        response.setHeader(header[i].getName(), header[i].getValue());
      }
    }

    copyServerResponse(response, method);
  }

  protected void copyServerResponse(
      HttpServletResponse response,
      HttpMethod method) throws IOException
  {

    InputStream bodyStream = method.getResponseBodyAsStream();

    // any response?
    if (bodyStream == null)
    {
      log.debug("No request body");
      return;
    }

    byte[] buffer = new byte[2048];
    int numBytes;
    OutputStream out = response.getOutputStream();

    // copy the response
    while ( (numBytes = bodyStream.read(buffer)) != -1)
    {
      out.write(buffer, 0, numBytes);
      if (log.isDebugEnabled())
      {
        log.debug("Copied " + numBytes + " bytes");
      }
    }
  }

  protected void registerMBeans()
  {
    MBeanServer mbs = MBeanServerLocator.locateJBoss();
    try
    {
      mbs.registerMBean(scheduler, schedulerName);
    }
    catch (Exception ex)
    {
      log.error("Failed to register MBeans", ex);
    }
  }

  protected void unregisterMBeans()
  {
    MBeanServer mbs = MBeanServerLocator.locateJBoss();
    try
    {
      mbs.unregisterMBean(schedulerName);
    }
    catch (Exception ex)
    {
      log.error("Failed to unregister MBeans", ex);
    }
  }

  // MBean Interface
  /**
   * Get the currently used connection timeout to slave hosts.
   * @jmx:managed-attribute
   */
  public int getConnectionTimeout()
  {
    return this.connectionTimeout;
  }

  /**
   * Set the currently used connection timeout to slave hosts.
   * @jmx:managed-attribute
   */
  public void setConnectionTimeout(int newTimeout)
  {
    this.connectionTimeout = newTimeout;
  }

  /**
   * Get the currently used connections to slave hosts.
   * @jmx:managed-attribute
   */
  public int getConnectionsInUse()
  {
    return connectionManager.getConnectionsInUse();
  }
}