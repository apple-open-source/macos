/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.util;

import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.Date;

import javax.management.MBeanServer;
import javax.management.ObjectName;

import org.jboss.mx.server.ServerConstants;

/**
 * Utility class for creating JMX agent identifiers. Also contains the
 * helper method for retrieving the <tt>AgentID</tt> of an existing MBean server
 * instance.
 *
 * @see javax.management.MBeanServerDelegateMBean
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.2 $
 *   
 */
public class AgentID 
   implements ServerConstants
{

   // Static ----------------------------------------------------
   private static int idSequence = 1;

   /**
    * Creates a new agent ID string. The identifier is of the form
    * <tt>&lt;ip.address&gt;/&lt;creation time in ms&gt;/&lt;sequence #&gt;</tt>
    *
    * @return Agent ID string
    */
   public static String create()
   {
      String ipAddress = null;
      
      try
      {
         ipAddress = InetAddress.getLocalHost().getHostAddress();
      }
      catch (UnknownHostException e) 
      {
         ipAddress = "127.0.0.1";
      }
      
      return ipAddress + "/" + System.currentTimeMillis() + "/" + (idSequence++);               
   }
   
   /**
    * Returns the agent identifier string of a given MBean server instance.
    *
    * @return <tt>MBeanServerId</tt> attribute of the MBean server delegate.
    */
   public static String get(MBeanServer server)
   {
      try 
      {
         ObjectName name = new ObjectName(MBEAN_SERVER_DELEGATE);
         String agentID = (String)server.getAttribute(name, "MBeanServerId");   
      
         return agentID;
      }
      catch (Throwable t)
      {
         throw new Error("Cannot find the MBean server delegate: " + t.toString());
      }
   }
}
      



