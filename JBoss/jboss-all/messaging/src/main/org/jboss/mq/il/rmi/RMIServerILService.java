/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mq.il.rmi;

import java.util.Properties;
import javax.naming.InitialContext;
import org.jboss.mq.GenericConnectionFactory;

import org.jboss.mq.il.ServerIL;
import org.jboss.mq.il.ServerILJMXService;
import org.jboss.mq.server.JMSServerInterceptor;
import org.jboss.mq.il.ServerILFactory;

/**
 *  Implements the ServerILJMXService which is used to manage the JVM IL.
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @version    $Revision: 1.12.2.1 $
 *
 * @jmx:mbean extends="org.jboss.mq.il.ServerILJMXServiceMBean"
 */
public class RMIServerILService extends org.jboss.mq.il.ServerILJMXService implements RMIServerILServiceMBean
{
   RMIServerIL serverIL;

   /**
    *  Gives this JMX service a name.
    *
    * @return    The Name value
    */
   public String getName()
   {
      return "JBossMQ-JVMServerIL";
   }

   /**
    *  Used to construct the GenericConnectionFactory (bindJNDIReferences()
    *  builds it)
    *
    * @return     The ServerIL value
    * @returns    ServerIL the instance of this IL
    */
   public ServerIL getServerIL()
   {
      return serverIL;
   }

   /**
    *  Used to construct the GenericConnectionFactory (bindJNDIReferences()
    *  builds it) Sets up the connection properties need by a client to use this
    *  IL
    *
    * @return    The ClientConnectionProperties value
    */
   public java.util.Properties getClientConnectionProperties()
   {
      Properties rc = super.getClientConnectionProperties();
      rc.setProperty(ServerILFactory.CLIENT_IL_SERVICE_KEY, "org.jboss.mq.il.rmi.RMIClientILService");
      return rc;
   }

   /**
    *  Starts this IL, and binds it to JNDI
    *
    * @exception  Exception  Description of Exception
    */
   public void startService() throws Exception
   {
      super.startService();
      serverIL = new RMIServerIL(lookupJMSServer());
      bindJNDIReferences();

   }

   /**
    *  Stops this IL, and unbinds it from JNDI
    */
   public void stopService() throws Exception
   {
      try
      {
         unbindJNDIReferences();
      }
      catch (Exception e)
      {
         e.printStackTrace();
      }
      super.stopService();
      serverIL = null;
   }
}
