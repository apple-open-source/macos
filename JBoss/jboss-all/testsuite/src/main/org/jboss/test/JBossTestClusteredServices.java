/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test;


import java.util.Vector;
import java.util.Hashtable;
import java.net.InetAddress;

import javax.management.ObjectName;
import javax.naming.InitialContext;

import org.jboss.jmx.adaptor.rmi.RMIAdaptor;

/**
 * Derived implementation of JBossTestServices for cluster testing.
 *
 * @see org.jboss.test.JBossTestServices
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.4.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>12 avril 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class JBossTestClusteredServices extends JBossTestServices
{
   
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   protected RMIAdaptor[] servers = null;
   
   // Static --------------------------------------------------------
   
   protected static String[] serverNames = {"jnp://localhost:1099", "jnp://localhost:21099"};
   protected static Hashtable[] targets = null;
   
   static 
   {
      // UGLY (but a good start...)
      Hashtable defProps = new Hashtable ();
      defProps.put ("java.naming.factory.initial", "org.jnp.interfaces.NamingContextFactory");
      defProps.put ("java.naming.factory.url.pkgs", "org.jnp.interfaces");
      
      Vector tgs = new Vector (serverNames.length);
      
      for (int i=0; i<serverNames.length; i++)
      {
         java.util.Hashtable aServer = (java.util.Hashtable)defProps.clone();
         aServer.put ("java.naming.provider.url", serverNames[i]);
         tgs.add (aServer);         
      }
      
      targets = new java.util.Hashtable[serverNames.length]; 
      targets = (java.util.Hashtable[])tgs.toArray (targets);
   }
   
   // Constructors --------------------------------------------------
   
   public JBossTestClusteredServices(String className)
   {
      super (className);
   }
   
   // Public --------------------------------------------------------
   
   // Z implementation ----------------------------------------------
   
   // Y overrides ---------------------------------------------------
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   RMIAdaptor[] getServers () throws Exception
   {
      init();
      return servers;
   }
   
   protected Object invoke (ObjectName name, String method, Object[] args, String[] sig) throws Exception
   {
      System.out.println("in invoke!!");
      RMIAdaptor[] servers = getServers();
      
      Object result = null;
      for (int i=0; i<servers.length; i++)
      {
         result = invoke (servers[i], name, method, args, sig);
      }
      
      return result;
      
   }
   
   protected void init() throws Exception
   {
      if (initialContext == null)
      {
         initialContext = new InitialContext();
      }
      if (servers == null)
      {         
         String serverName = System.getProperty("jbosstest.server.name");
         if (serverName == null)
         {
            serverName = InetAddress.getLocalHost().getHostName();
         }
         
         java.util.Vector servList = new java.util.Vector (targets.length);
         for (int i=0; i<targets.length; i++)
         {
            InitialContext tmpCtx = new InitialContext (targets[i]);
            servList.add (tmpCtx.lookup ("jmx:" + serverName + ":rmi"));            
         }
         
         servers = new RMIAdaptor[targets.length]; 
         servers = (RMIAdaptor[])servList.toArray (servers);
      }
   }

   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
}
