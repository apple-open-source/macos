/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.tm.usertx.server;

import java.rmi.server.UnicastRemoteObject;

import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.naming.InitialContext;
import javax.naming.Context;

import org.jboss.system.ServiceMBeanSupport;
import org.jboss.tm.usertx.client.ClientUserTransaction;
import org.jboss.tm.usertx.interfaces.UserTransactionSessionFactory;

/**
 *  This is a JMX service handling the serverside of UserTransaction
 *  usage for standalone clients.
 *
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.6.2.2 $
 */
public class ClientUserTransactionService
      extends ServiceMBeanSupport
      implements ClientUserTransactionServiceMBean
{

   // Constants -----------------------------------------------------

   public static String JNDI_NAME = "UserTransaction";
   public static String FACTORY_NAME = "UserTransactionSessionFactory";


   // Attributes ----------------------------------------------------

   MBeanServer server;

   // Keep a reference to avoid DGC.
   private UserTransactionSessionFactory factory;


   // ServiceMBeanSupport overrides ---------------------------------

   protected ObjectName getObjectName(MBeanServer server, ObjectName name)
         throws javax.management.MalformedObjectNameException
   {
      this.server = server;
      return OBJECT_NAME;
   }

   protected void startService()
         throws Exception
   {
      factory = new UserTransactionSessionFactoryImpl();

      Context ctx = new InitialContext();
      ctx.bind(FACTORY_NAME, factory);
      ctx.bind(JNDI_NAME, ClientUserTransaction.getSingleton());
   }

   protected void stopService()
   {
      try
      {
         Context ctx = new InitialContext();
         ctx.unbind(FACTORY_NAME);
         ctx.unbind(JNDI_NAME);

         // Force unexport, and drop factory reference.
         try
         {
            UnicastRemoteObject.unexportObject(factory, true);
         }
         catch (Exception ex)
         {
            log.error("Failed to unexportObject", ex);
         }
         factory = null;
      }
      catch (Exception e)
      {
         log.error("Failed to unbind", e);
      }
   }

}
