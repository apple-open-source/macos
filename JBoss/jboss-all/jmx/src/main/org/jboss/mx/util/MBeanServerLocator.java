/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.mx.util;

import java.util.Iterator;

import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
   
/**
 * A helper class to locate a MBeanServer.
 *      
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @version $Revision: 1.1.2.2 $
 */
public class MBeanServerLocator
{
   public static MBeanServer locate(final String agentID)
   {
      MBeanServer server = (MBeanServer)
         MBeanServerFactory.findMBeanServer(agentID).iterator().next();
      
      return server;
   }

   public static MBeanServer locate()
   {
      return locate(null);
   }

   public static MBeanServer locateJBoss()
   {
      for (Iterator i = MBeanServerFactory.findMBeanServer(null).iterator(); i.hasNext(); )
      {
          MBeanServer server = (MBeanServer) i.next();
          if (server.getDefaultDomain().equals("jboss"))
             return server;
      }
      throw new IllegalStateException("No 'jboss' MBeanServer found!");
   }
}
