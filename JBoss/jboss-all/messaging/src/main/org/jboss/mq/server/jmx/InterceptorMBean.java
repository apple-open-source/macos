/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.mq.server.jmx;

import javax.management.ObjectName;

import org.jboss.mq.server.JMSServerInterceptor;
import org.jboss.system.ServiceMBean;
/**
 * Mbean interface for JMBossMQ to help 
 * load an JMSServerInterceptor
 *
 * @author <a href="mailto:cojonudo14@hotmail.com">Hiram Chirino</a>
 * @author <a href="mailto:pra@tim.se">Peter Antman</a>
 * @version $Revision: 1.1.4.2 $
 */

public interface InterceptorMBean extends ServiceMBean
{

   JMSServerInterceptor getInterceptor();

   /**
    * Gets the next interceptor in the chain
    * @param v  Value to assign to nextInterceptor.
    */
   ObjectName getNextInterceptor();

   /**
    * Set the next interceptor in the chain
    * @param v  Value to assign to nextInterceptor
    */
   void setNextInterceptor(ObjectName jbossMQService);

} // JBossMQServiceAdapterMBean
