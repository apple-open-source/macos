/*
 * JBoss, the OpenSource J2EE webOS
 * 
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mq.server.jmx;

import org.jboss.mq.server.JMSServerInterceptor;

/**
 * JMX MBean implementation DelayInterceptor.
 *
 * @jmx:mbean extends="org.jboss.mq.server.jmx.InterceptorMBean"
 * @author     <a href="hiram.chirino@jboss.org">Hiram Chirino</a>
 * @version    $Revision: 1.3 $
 */
public class DelayInterceptor
   extends InterceptorMBeanSupport 
   implements DelayInterceptorMBean {

   private org.jboss.mq.server.DelayInterceptor delayInterceptor
      = new org.jboss.mq.server.DelayInterceptor();
      
   public JMSServerInterceptor getInterceptor()
   {
      return delayInterceptor;
   }

   /**
    * @jmx:managed-attribute
    */
   public boolean getDelayEnabled()
   {
      return delayInterceptor.delayEnabled;
   }

   /**
    * @jmx:managed-attribute
    */
   public long getDelayTime()
   {
      return delayInterceptor.delayTime;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setDelayEnabled(boolean delayEnabled)
   {
      delayInterceptor.delayEnabled = delayEnabled;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setDelayTime(long delayTime)
   {
      delayInterceptor.delayTime = delayTime;
   }

}
