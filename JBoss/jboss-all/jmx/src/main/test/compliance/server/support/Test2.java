/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.server.support;

import javax.management.MBeanRegistration;
import javax.management.ObjectName;
import javax.management.MBeanServer;

/**
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.2 $
 *   
 */
public class Test2 implements Test2MBean, MBeanRegistration
{

   public ObjectName preRegister(MBeanServer server, ObjectName name) throws Exception
   {
      throw new RuntimeException();
   }
   
   public void postRegister(Boolean b)
   {
   }
   
   public void preDeregister() throws Exception
   {
   }
   
   public void postDeregister()
   {
      
   }
}




