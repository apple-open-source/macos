/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.jmx.adaptor.rmi;

import java.net.InetAddress;

import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.MalformedObjectNameException;

import javax.naming.InitialContext;

import org.jboss.naming.Util;
import org.jboss.system.ServiceMBeanSupport;

/**
 * A JMX RMI Adapter service.
 *
 * @jmx:mbean name="jboss.jmx:type=adaptor,protocol=RMI"
 *            extends="org.jboss.system.ServiceMBean"
 *
 * @version <tt>$Revision: 1.5.2.1 $</tt>
 * @author  <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author  <A href="mailto:andreas.schaefer@madplanet.com">Andreas &quot;Mad&quot; Schaefer</A>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 **/
public class RMIAdaptorService
   extends ServiceMBeanSupport
   implements RMIAdaptorServiceMBean
{
   // Constants -----------------------------------------------------
   public static final String JMX_NAME = "jmx";
   public static final String PROTOCOL_NAME = "rmi";
   public static final String DEFAULT_JNDI_NAME = "jmx/rmi/RMIAdaptor";

   /** The RMI adapter instance. */
   private RMIAdaptor adaptor;

   /** The InetAddress localhost name used for the legacy JNDI name */
   private String host;

   /** The legacy JNDI suffix or null for none. */
   private String name;

   /** The user supplied JNDI name */
   private String jndiName = DEFAULT_JNDI_NAME;

   /** The port the container will be exported on */
   private int rmiPort = 0;
   /** The connect backlog for the rmi listening port */
   private int backlog = 50;
   /** The address to bind the rmi port on */
   protected String serverAddress;

   /**
    * @jmx:managed-constructor
    */
   public RMIAdaptorService(String name)
   {
      this.name = name;
   }

   /**
    * @jmx:managed-constructor
    */
   public RMIAdaptorService()
   {
      this(null);
   }

   /**
    * @jmx:managed-attribute
    */
   public void setJndiName(final String jndiName)
   {
      this.jndiName = jndiName;
   }

   /**
    * @jmx:managed-attribute
    */
   public String getJndiName()
   {
      return jndiName;
   }

   /**
    * @jmx:managed-attribute
    */
   public int getBacklog()
   {
      return backlog;
   }
   /**
    * @jmx:managed-attribute
    */
   public void setBacklog(int backlog)
   {
      this.backlog = backlog;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setRMIObjectPort(final int rmiPort)
   {
      this.rmiPort = rmiPort;
   }
   /**
    * @jmx:managed-attribute
    */
   public int getRMIObjectPort()
   {
      return rmiPort;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setServerAddress(final String address)
   {
      this.serverAddress = address;
   }
   /**
    * @jmx:managed-attribute
    */
   public String getServerAddress()
   {
      return serverAddress;
   }

   /** The legacy hard-coded name. Get rid of this in the future
    * @jmx:managed-attribute
    * @deprecated
    */
   public String getLegacyJndiName()
   {
      if (name != null)
      {
         return JMX_NAME + ":" + host + ":" + PROTOCOL_NAME + ":" + name;
      }
      else
      {
         return JMX_NAME + ":" + host + ":" + PROTOCOL_NAME;
      }
   }

   ///////////////////////////////////////////////////////////////////////////
   //                    ServiceMBeanSupport Overrides                      //
   ///////////////////////////////////////////////////////////////////////////

   protected void startService() throws Exception
   {
      // Setup the RMI server object
      InetAddress bindAddress = null;
      if( serverAddress != null && serverAddress.length() > 0 )
         bindAddress = InetAddress.getByName(serverAddress);
      adaptor = new RMIAdaptorImpl(getServer(), rmiPort, bindAddress, backlog);
      log.debug("Created RMIAdaptorImpl: "+adaptor);
      InitialContext iniCtx = new InitialContext();

      // Bind the RMI object under the JndiName attribute
      Util.bind(iniCtx, jndiName, adaptor);
      // Bind under the hard-coded legacy name for compatibility
      host = InetAddress.getLocalHost().getHostName();
      String legacyName = getLegacyJndiName();
      iniCtx.bind(legacyName, adaptor);
   }

   protected void stopService() throws Exception
   {
      InitialContext ctx = new InitialContext();

      try
      {
         ctx.unbind(getJndiName());
         ctx.unbind(getLegacyJndiName());
      }
      finally
      {
         ctx.close();
      }
   }
}
