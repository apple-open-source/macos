package org.jboss.invocation.http.server;

import java.net.URL;

import org.jboss.invocation.Invocation;
import org.jboss.system.ServiceMBean;

/** The MBean interface for the HTTP invoker.
 @author Scott.Stark@jboss.org
 @version $Revision: 1.1.2.3 $
 */
public interface HttpInvokerMBean extends ServiceMBean
{
   /** Get the URL string of the servlet that will handle posts from
    * the HttpInvokerProxy
    */
   public String getInvokerURL();
   /** Get the URL string of the servlet that will handle posts from
    * the HttpInvokerProxy. For example, http://webhost:8080/invoker/JMXInvokerServlet
    */
   public void setInvokerURL(String invokerURL);

   /** If there is no invokerURL set, then one will be constructed via the
    * concatenation of invokerURLPrefix + the local host + invokerURLSuffix.
    */
   public String getInvokerURLPrefix();
   /** If there is no invokerURL set, then one will be constructed via the
    * concatenation of invokerURLPrefix + the local host + invokerURLSuffix.
    * An example prefix is "http://", and this is the default value.
    */
   public void setInvokerURLPrefix(String invokerURLPrefix);

   /** If there is no invokerURL set, then one will be constructed via the
    * concatenation of invokerURLPrefix + the local host + invokerURLSuffix.
    */
   public String getInvokerURLSuffix();
   /** If there is no invokerURL set, then one will be constructed via the
    * concatenation of invokerURLPrefix + the local host + invokerURLSuffix.
    * An example suffix is "/invoker/JMXInvokerServlet" and this is the default
    * value.
    */
   public void setInvokerURLSuffix(String invokerURLSuffix);

   /** A flag if the InetAddress.getHostName() or getHostAddress() method
    * should be used as the host component of invokerURLPrefix + host +
    * invokerURLSuffix. If true getHostName() is used, false getHostAddress().
    */
   public boolean getUseHostName();
   /** A flag if the InetAddress.getHostName() or getHostAddress() method
    * should be used as the host component of invokerURLPrefix + host +
    * invokerURLSuffix. If true getHostName() is used, false getHostAddress().
    */
   public void setUseHostName(boolean flag);

   /** The invoker JMX method
   */
   public Object invoke(Invocation invocation)
      throws Exception;
}
