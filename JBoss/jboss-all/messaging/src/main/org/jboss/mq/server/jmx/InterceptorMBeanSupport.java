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
 * @author <a href="Peter Antman">Peter Antman</a>
 * @version $Revision: 1.1 $
 */

abstract public class InterceptorMBeanSupport  
   extends ServiceMBeanSupport
   implements InterceptorMBean
{
   /**
    * The next interceptor this interceptor invokes.
    */
   private JMSServerInterceptor nextInterceptor;
   private ObjectName nextInterceptorObjName;
   
  
   public ObjectName getNextInterceptor() 
   {
      return this.nextInterceptorObjName;
   }
   
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
      getInterceptor().setNext(nextInterceptor);
   }
   
} 
