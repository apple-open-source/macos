/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.interceptor;

import javax.management.MBeanInfo;
import org.jboss.mx.server.MBeanInvoker;

/**
 * A rather useless Log interceptor.
 *
 * @see org.jboss.mx.interceptor.AbstractInterceptor
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.2.8.2 $
 *   
 */
public final class LogInterceptor
   extends AbstractInterceptor
{
   // Public --------------------------------------------------------
   public LogInterceptor(MBeanInfo info, MBeanInvoker invoker)
   {
      super(info, invoker);
   }
}
