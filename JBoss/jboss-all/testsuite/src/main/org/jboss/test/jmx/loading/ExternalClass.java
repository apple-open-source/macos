/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jmx.loading;

import org.jboss.system.ServiceMBean;
import org.jboss.system.ServiceMBeanSupport;

import org.jboss.test.jmx.loading.util.Util;

/** An mbean service that does nothing but references an external
 * class that is loaded from a jar refereced by the sar manifest
 *
 * @author Scott.Stark@jboss.org
 * @version  $Revision: 1.2 $
 */
public class ExternalClass extends ServiceMBeanSupport
   implements ExternalClassMBean
{
   private static Util u = new Util();

   public String getName()
   {
      return "ExternalClass";
   }

}
