/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.server.support;

import javax.management.MBeanRegistration;
import javax.management.MBeanServer;
import javax.management.ObjectName;

/**
 * MBean that disallows unregistration by throwing an exception from its
 * <tt>preDeregister()</tt> method.
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.2 $
 *   
 */
public class LockedTest
         implements LockedTestMBean, MBeanRegistration
{

   public ObjectName preRegister(MBeanServer server, ObjectName name) throws Exception
   {
      return name;
   }
   
   public void postRegister(Boolean b)
   {
   
   }
   
   public void preDeregister() throws Exception
   {
      throw new RuntimeException("You cannot unregister me!");
   }
   
   public void postDeregister()
   {
   
   }
}




