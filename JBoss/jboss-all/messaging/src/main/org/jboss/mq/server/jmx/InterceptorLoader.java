/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.mq.server.jmx;
import javax.jms.IllegalStateException;
import javax.management.ObjectName;

import org.jboss.mq.server.JMSServerInterceptor;
import org.jboss.system.ServiceMBeanSupport;
/**
 * Adapts JBossMQService to deliver JMSServerInvoker.
 *
 *
 * @jmx:mbean extends="org.jboss.mq.server.jmx.InterceptorMBean"
 *
 * @author <a href="Peter Antman">Peter Antman</a>
 * @version $Revision: 1.2 $
 */

public class InterceptorLoader  
   extends InterceptorMBeanSupport
   implements InterceptorLoaderMBean
{
   /**
    * The invoker this service boostraps
    */
   private JMSServerInterceptor interceptor;
  
   /**
    * Gets the JMSServer attribute of the JBossMQService object, here we
    * wrap the server in the invoker loaded.
    *
    * @return    The JMSServer value
    */
   public JMSServerInterceptor getInterceptor()
   {
      return interceptor;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setInterceptorClass(String c) throws Exception {
      interceptor = (JMSServerInterceptor)Thread.currentThread().getContextClassLoader().loadClass(c).newInstance();
   }
   
   /**
    * @jmx:managed-attribute
    */
   public String getInterceptorClass() throws Exception {
      return interceptor.getClass().getName();
   }

   protected void startService() throws Exception
   {
      if( interceptor==null )
         throw new IllegalStateException("The interceptor class was not set (null)"); 
         
      super.startService();
   }
} // JBossMQServiceAdapter
