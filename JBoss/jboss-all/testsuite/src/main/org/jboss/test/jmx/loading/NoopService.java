/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jmx.loading;

import org.jboss.system.ServiceMBean;
import org.jboss.system.ServiceMBeanSupport;

/** This service that does nothing
 *
 * @author Scott.Stark@jboss.org
 * @version  $Revision: 1.2 $
 */
public class NoopService extends ServiceMBeanSupport implements NoopServiceMBean
{
   public String getName()
   {
      return "NoopService";
   }

}
