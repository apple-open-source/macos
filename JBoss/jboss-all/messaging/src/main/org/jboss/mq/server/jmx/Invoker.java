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
import org.jboss.mq.server.JMSServerInvoker;
import org.jboss.system.ServiceMBeanSupport;
/**
 * Adapts JBossMQService to deliver JMSServerInvoker.
 *
 * @jmx:mbean extends="org.jboss.system.ServiceMBean"
 * 
 * @author <a href="Peter Antman">Peter Antman</a>
 * @version $Revision: 1.2 $
 */

public class Invoker  
   extends ServiceMBeanSupport
   implements InvokerMBean
{
   /**
    * The next interceptor this interceptor invokes.
    */
   private JMSServerInterceptor nextInterceptor;
   private ObjectName nextInterceptorObjName;
   private JMSServerInvoker invoker;
  

   /**
    * @jmx:managed-attribute
    */
   public ObjectName getNextInterceptor() 
   {
      return this.nextInterceptorObjName;
   }
   
   /**
    * @jmx:managed-attribute
    */
   public void setNextInterceptor(ObjectName  jbossMQService) 
   {
      this.nextInterceptorObjName = jbossMQService;
   }
   
   protected void startService() throws Exception
   {
      if( nextInterceptorObjName != null ) {
         nextInterceptor = (JMSServerInterceptor)getServer().getAttribute(nextInterceptorObjName, "Interceptor");
         if (nextInterceptor == null) 
            throw new IllegalStateException("The next interceptor was invalid.");         
      }
      invoker.setNext(nextInterceptor);
   }
   

   /**
    * @see ServiceMBeanSupport#createService()
    */
   protected void createService() throws Exception
   {
      super.createService();
      invoker = new JMSServerInvoker();
   }

   /**
    * @jmx:managed-attribute
    * @see InvokerMBean#getInvoker()
    */
   public JMSServerInvoker getInvoker()
   {
      return invoker;
   }

} 
