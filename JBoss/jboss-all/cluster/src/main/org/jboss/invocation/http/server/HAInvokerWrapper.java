/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.invocation.http.server;

import java.util.ArrayList;
import javax.management.JMException;
import javax.management.MBeanServer;
import javax.management.MBeanException;
import javax.management.ObjectName;
import javax.management.ReflectionException;

import org.jboss.ha.framework.interfaces.HARMIResponse;
import org.jboss.ha.framework.interfaces.GenericClusteringException;
import org.jboss.ha.framework.server.HATarget;
import org.jboss.invocation.Invocation;
import org.jboss.logging.Logger;
import org.jboss.mx.util.JMXExceptionDecoder;
import org.jboss.mx.util.DynamicMBeanSupport;


/** This is an invoker that delegates to the target invoker and handles the
 * wrapping of the response in an HARMIResponse with any updated HATarget info.
 * @see HttpProxyFactoryHA
 *
 * @author <a href="mailto:scott.stark@jboss.org">Scott Stark</a>
 * @version $Revision: 1.1.6.2 $
 */
public class HAInvokerWrapper extends DynamicMBeanSupport
{
   private static Logger log = Logger.getLogger(HAInvokerWrapper.class);
   private MBeanServer mbeanServer;
   private ObjectName targetName;
   private HATarget target;

   public HAInvokerWrapper(MBeanServer mbeanServer, ObjectName targetName, HATarget target)
   {
      this.mbeanServer = mbeanServer;
      this.targetName = targetName;
      this.target = target;
   }

   /** The JMX DynamicMBean invoke entry point. This only handles the
    * invoke(Invocation) operation.
    *
    * @param actionName
    * @param params
    * @param signature
    * @return
    * @throws MBeanException
    * @throws ReflectionException
    */
   public Object invoke(String actionName, Object[] params, String[] signature)
      throws MBeanException, ReflectionException
   {
      if( params == null || params.length != 1 ||
         (params[0] instanceof Invocation) == false )
      {
         NoSuchMethodException e = new NoSuchMethodException(actionName);
         throw new ReflectionException(e, actionName);
      }

      Invocation invocation = (Invocation) params[0];
      try
      {
         Object value = invoke(invocation);
         return value;
      }
      catch(Exception e)
      {
         throw new ReflectionException(e, "Invoke failure");
      }
   }

   /** The invoker entry point.
    * @param invocation
    * @return A HARMIResponse that wraps the result of calling invoke(Invocation)
    * on the targetName MBean
    * @throws Exception
    */
   public Object invoke(Invocation invocation)
      throws Exception
   {
      ClassLoader oldCl = Thread.currentThread().getContextClassLoader();
      try
      {
         // The cl on the thread should be set in another interceptor
         Object[] args = {invocation};
         String[] sig = {"org.jboss.invocation.Invocation"};
         Object rtn = mbeanServer.invoke(targetName, "invoke", args, sig);

         // Update the targets list if the client view is out of date
         Long clientViewId = (Long) invocation.getValue("CLUSTER_VIEW_ID");
         HARMIResponse rsp = new HARMIResponse();
         if (clientViewId.longValue() != target.getCurrentViewId())
         {
            rsp.newReplicants = new ArrayList(target.getReplicants());
            rsp.currentViewId = target.getCurrentViewId();
         }
         rsp.response = rtn;

         // Return the raw object and let the http layer marshall it
         return rsp;
      }
      catch (Exception e)
      {
         // Unwrap any JMX exceptions
         e = (Exception) JMXExceptionDecoder.decode(e);
         // Don't send JMX exception back to client to avoid needing jmx
         if( e instanceof JMException )
            e = new GenericClusteringException (GenericClusteringException.COMPLETED_NO, e.getMessage());

         // Only log errors if trace is enabled
         if( log.isTraceEnabled() )
            log.trace("operation failed", e);
         throw e;
      }
      finally
      {
         Thread.currentThread().setContextClassLoader(oldCl);
      }
   }

}
