/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: JMXEngineConfigurationProvider.java,v 1.1.2.2 2003/06/16 20:50:39 ejort Exp $

package org.jboss.net.axis.server;

import java.util.Iterator;

import javax.management.JMException;
import javax.management.ObjectName;
import javax.management.MBeanServer;

import org.apache.axis.EngineConfiguration;
import org.apache.axis.server.AxisServer;
import org.jboss.net.axis.EngineConfigurationProvider;
import org.jboss.mx.util.MBeanServerLocator;

/**
 * configuration provider that accesses configuration services via JMX.
 * @author jung
 * @version $Revision: 1.1.2.2 $
 * @created 9.9.2002
 */
public class JMXEngineConfigurationProvider
   implements EngineConfigurationProvider {

   //
   // Attributes
   //
   
   public static JMXEngineConfigurationProvider jecp=new JMXEngineConfigurationProvider();

   //
   // Protected Helpers
   //
   
   /**
    * find attribute through JMX server and mbean
    */
   protected Object getAttribute(String context, String attributeName)
   {
      try
      {
         ObjectName mBeanName = new ObjectName(context);
         MBeanServer server = MBeanServerLocator.locateJBoss();
         return server.getAttribute(mBeanName, attributeName);
      }
      catch(JMException e)
      {
      }
      
      return null;
   }

   //
   // Public API
   //
   
   /**
    * return client config associated with mbean 
    * @see org.jboss.net.axis.EngineConfigurationProvider#getClientEngineConfiguration(String)
    */
   public EngineConfiguration getClientEngineConfiguration(String context) {
      return (EngineConfiguration) getAttribute(context,"ClientEngineConfiguration");
   }
   
   /** return server config associated with mbean */
   public EngineConfiguration getServerEngineConfiguration(String context) {
      return (EngineConfiguration) getAttribute(context,"ServerEngineConfiguration");
   }

   /** return axis server associated with mbean */
   public AxisServer getAxisServer(String context) {
      return (AxisServer) getAttribute(context,"AxisServer");
   }
   
}
