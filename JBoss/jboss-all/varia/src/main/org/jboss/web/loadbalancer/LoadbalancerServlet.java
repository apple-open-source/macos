/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.loadbalancer;

import java.io.IOException;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.apache.commons.httpclient.HttpMethod;
import org.jboss.logging.Logger;
import org.jboss.web.loadbalancer.scheduler.NoHostAvailableException;
import org.jboss.web.loadbalancer.util.Constants;
import org.jboss.mx.util.MBeanProxyExt;
import javax.management.MalformedObjectNameException;

/**
 * A servlet that does the job of a reverse proxy.
 *
 * @author Thomas Peuss <jboss@peuss.de>
 * @version $Revision: 1.4.2.1 $
 */
public class LoadbalancerServlet
    extends HttpServlet
{
  protected static Logger log = Logger.getLogger(LoadbalancerServlet.class);

  protected String loadbalancerName;
  protected LoadbalancerServiceMBean loadbalancer;

  protected void handleRequest(
      HttpServletRequest request,
      HttpServletResponse response,
      int requestMethod) throws ServletException, IOException
  {
    HttpMethod method = null;

    initLoadbalancerDelegate();

    // get target host from scheduler
    try
    {
      method = loadbalancer.createMethod(request, response, requestMethod);
    }
    catch (NoHostAvailableException nhae)
    {
      log.error("We have no host to schedule request - giving up");
      response.sendError(HttpServletResponse.SC_SERVICE_UNAVAILABLE, nhae.getMessage());
      return;
    }
    loadbalancer.addRequestData(request, method);

    // do handle the request
    loadbalancer.handleRequest(request, response, method);
  }

  /* Handler for GET-requests
   * @see javax.servlet.http.HttpServlet#doGet(javax.servlet.http.HttpServletRequest, javax.servlet.http.HttpServletResponse)
   */
  protected void doGet(
      HttpServletRequest request,
      HttpServletResponse response) throws ServletException, IOException
  {
    handleRequest(request, response,Constants.HTTP_METHOD_GET);
  }

  /* Handler for POST-requests
   * @see javax.servlet.http.HttpServlet#doPost(javax.servlet.http.HttpServletRequest, javax.servlet.http.HttpServletResponse)
   */
  protected void doPost(
      HttpServletRequest request,
      HttpServletResponse response) throws ServletException, IOException
  {
    handleRequest(request, response, Constants.HTTP_METHOD_POST);
  }

  /* Handler for DELETE-requests
   * @see javax.servlet.http.HttpServlet#doDelete(javax.servlet.http.HttpServletRequest, javax.servlet.http.HttpServletResponse)
   */
  protected void doDelete(HttpServletRequest request, HttpServletResponse response) throws
      ServletException, IOException
  {
    handleRequest(request, response, Constants.HTTP_METHOD_DELETE);
  }

  /* Handler for HEAD-requests
   * @see javax.servlet.http.HttpServlet#doHead(javax.servlet.http.HttpServletRequest, javax.servlet.http.HttpServletResponse)
   */
  protected void doHead(HttpServletRequest request, HttpServletResponse response) throws
      ServletException, IOException
  {
    handleRequest(request, response, Constants.HTTP_METHOD_HEAD);
  }

  /* Handler for OPTIONS-requests
   * @see javax.servlet.http.HttpServlet#doOptions(javax.servlet.http.HttpServletRequest, javax.servlet.http.HttpServletResponse)
   */
  protected void doOptions(HttpServletRequest request, HttpServletResponse response) throws
      ServletException, IOException
  {
    handleRequest(request, response, Constants.HTTP_METHOD_OPTIONS);
  }

  /* Handler for PUT-requests
   * @see javax.servlet.http.HttpServlet#doPut(javax.servlet.http.HttpServletRequest, javax.servlet.http.HttpServletResponse)
   */
  protected void doPut(HttpServletRequest request, HttpServletResponse response) throws
      ServletException, IOException
  {
    handleRequest(request, response, Constants.HTTP_METHOD_PUT);
  }

  /* Servlet initialisation.
   * @see javax.servlet.Servlet#init(javax.servlet.ServletConfig)
   */
  public void init(ServletConfig config) throws ServletException
  {
    super.init(config);
    loadbalancerName=config.getInitParameter("loadbalancer-service-name");

    log.debug("Servlet init ready");
  }

  /* Servlet deinit.
   * @see javax.servlet.Servlet#destroy()
   */
  public void destroy()
  {
    super.destroy();
    loadbalancer=null;
    log.debug("Servlet destroyed");
  }

  protected void initLoadbalancerDelegate()
  {
    if (loadbalancer == null)
    {
      try
      {
        loadbalancer = (LoadbalancerServiceMBean)
            MBeanProxyExt.create(LoadbalancerServiceMBean.class,
                                 loadbalancerName);
      }
      catch (MalformedObjectNameException ex)
      {
        log.error("Could not create LoadbalancerService-Proxy",ex);
      }

    }
  }
}