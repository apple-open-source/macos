/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.implementation.loading.support;

import javax.management.ObjectName;
import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;

/**
 * Start MBean invokes Target MBean with an arg AClass that
 * both MBeans have loaded using different MLet loaders.
 *
 * @see <related>
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.1 $
 *   
 */
public class Start
   implements StartMBean
{

   public void startOp(String agentID) throws Exception
   {
      MBeanServer server = (MBeanServer)MBeanServerFactory.findMBeanServer(agentID).get(0);
      
      server.invoke(new ObjectName(":name=Target"), "executeTarget",
      new Object[] { new AClass() },
      new String[] { AClass.class.getName() }
      );
   }
   
}
      



