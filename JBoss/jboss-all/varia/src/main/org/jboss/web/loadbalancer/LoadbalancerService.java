/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.loadbalancer;

import org.jboss.system.ServiceMBeanSupport;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import org.apache.commons.httpclient.HttpMethod;
import javax.servlet.ServletException;
import java.io.IOException;
import org.w3c.dom.Element;
import org.jboss.web.loadbalancer.scheduler.NoHostAvailableException;

/**
 *
 * @jmx:mbean name="jboss.web.loadbalancer: service=Loadbalancer"
 *            extends="org.jboss.system.ServiceMBean"
 *
 * @author Thomas Peuss <jboss@peuss.de>
 * @version $Revision: 1.1.2.1 $
 */
public class LoadbalancerService
    extends ServiceMBeanSupport
    implements LoadbalancerServiceMBean
{
  protected Element config;
  protected Loadbalancer loadbalancer;

  protected void startService() throws java.lang.Exception
  {
    loadbalancer.registerMBeans();
  }

  protected void destroyService() throws java.lang.Exception
  {
    loadbalancer = null;
  }

  protected void createService() throws java.lang.Exception
  {
    loadbalancer = new Loadbalancer(config);
  }

  protected void stopService() throws java.lang.Exception
  {
    loadbalancer.unregisterMBeans();
  }

  /**
   * Configure this service.
   * @jmx:managed-attribute
   * @param config
   */
  public void setConfig(Element config)
  {
    this.config = config;
  }

  /**
   * Get the currently used connection timeout to slave hosts.
   * @jmx:managed-attribute
   */
  public int getConnectionTimeout()
  {
    return loadbalancer.getConnectionTimeout();
  }

  /**
   * Set the currently used connection timeout to slave hosts.
   * @jmx:managed-attribute
   */
  public void setConnectionTimeout(int newTimeout)
  {
    loadbalancer.setConnectionTimeout(newTimeout);
  }

  /**
   * Get the currently used connections to slave hosts.
   * @jmx:managed-attribute
   */
  public int getConnectionsInUse()
  {
    return loadbalancer.getConnectionsInUse();
  }

  /**
   * @jmx:managed-operation
   */
  public HttpMethod createMethod(
      HttpServletRequest request,
      HttpServletResponse response,
      int requestMethod) throws NoHostAvailableException
  {
    return loadbalancer.createMethod(request,response,requestMethod);
  }

  /**
   * @jmx:managed-operation
   */
  public HttpMethod addRequestData(HttpServletRequest request,
                                      HttpMethod method)
  {
    return loadbalancer.addRequestData(request,method);
  }

  /**
   * @jmx:managed-operation
   */
  public void handleRequest(
      HttpServletRequest request,
      HttpServletResponse response,
      HttpMethod method) throws ServletException, IOException
  {
    loadbalancer.handleRequest(request, response, method);
  }
}