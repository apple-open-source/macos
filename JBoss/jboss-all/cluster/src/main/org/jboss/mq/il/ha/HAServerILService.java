/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.ha;

import java.util.Properties;
import javax.management.*;
import javax.naming.InitialContext;

import org.jboss.mq.GenericConnectionFactory;
import org.jboss.ha.framework.server.HARMIServerImpl;
import org.jboss.ha.framework.interfaces.HAPartition;
import org.jboss.ha.framework.interfaces.LoadBalancePolicy;

import org.jboss.mq.il.ServerIL;
import org.jboss.mq.il.ServerILJMXService;
import org.jboss.mq.server.JMSServerInvoker;
import org.jboss.mq.il.ServerILFactory;

/**
 *  Implements the ServerILJMXService which is used to manage the HA IL.
 *  Almost the same as the RMI IL except that we used the HARMIServerImpl 
 *  object to make the object HA via the jboss cluster.
 * 
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @version    $Revision: 1.6 $
 */
public class HAServerILService extends org.jboss.mq.il.ServerILJMXService implements HAServerILServiceMBean {

   public final static String DEFAULT_PARTITION = "DefaultPartition";

   protected ServerIL serverILProxy;
   protected HAServerIL serverIL;
   protected HAPartition partition;
   protected HARMIServerImpl harmi;
   protected String partitionName = DEFAULT_PARTITION;
   protected String loadBalancePolicy = "org.jboss.ha.framework.interfaces.FirstAvailable";
   protected Class loadBalancePolicyClass;

   /**
    *  Gives this JMX service a name.
    *
    * @return    The Name value
    */
   public String getName() {
      return "JBossMQ-HAServerIL";
   }

   /**
    *  Used to construct the GenericConnectionFactory (bindJNDIReferences()
    *  builds it)
    *
    * @return     The ServerIL value
    * @returns    ServerIL the instance of this IL
    */
   public ServerIL getServerIL() {
      return serverILProxy;
   }

   /**
    *  Used to construct the GenericConnectionFactory (bindJNDIReferences()
    *  builds it) Sets up the connection properties need by a client to use this
    *  IL
    *
    * @return    The ClientConnectionProperties value
    */
   public java.util.Properties getClientConnectionProperties() {
      Properties rc = super.getClientConnectionProperties();
      rc.setProperty(ServerILFactory.CLIENT_IL_SERVICE_KEY, "org.jboss.mq.il.ha.HAClientILService");
      return rc;
   }

   /**
    *  Starts this IL, and binds it to JNDI
    *
    * @exception  Exception  Description of Exception
    */
   public void startService() throws Exception {

      super.startService();

      ClassLoader cl = Thread.currentThread().getContextClassLoader();
      log.info("Using " + loadBalancePolicy + " for jms connection load balancing");
      loadBalancePolicyClass = cl.loadClass(loadBalancePolicy);

      // The serverIL object is the RMI object we are going to make HA
      serverIL = new HAServerIL(lookupJMSServer());

      partition = (HAPartition) new InitialContext().lookup("/HAPartition/" + partitionName);
      harmi = new HARMIServerImpl(partition, getConnectionFactoryJNDIRef(), ServerIL.class, serverIL);
      serverILProxy = (ServerIL) harmi.createHAStub((LoadBalancePolicy) loadBalancePolicyClass.newInstance());
      serverIL.setHaServerILProxy(serverILProxy);
      
      bindJNDIReferences();

      if (log.isDebugEnabled())
        log.debug("HAServerILProxy=" + serverILProxy);
   }

   /**
    *  Stops this IL, and unbinds it from JNDI
    */
   public void stopService() {
      try {
         unbindJNDIReferences();
         harmi.destroy();
		 super.stopService();
      } catch (Exception ex) {
         log.error("Error stopping HAServerIL", ex);
         throw new RuntimeException("Error stopping HAServerIL");
      }
   }

   //////////////////////////////////////////////////////////////////
   //
   // Methods used for managing this object via JMX 
   //
   //////////////////////////////////////////////////////////////////

   /**
    * Gets the partitionName
    * @return Returns a String
    */
   public String getPartitionName() {
      return partitionName;
   }
   /**
    * Sets the partitionName
    * @param partitionName The partitionName to set
    */
   public void setPartitionName(String partitionName) {
      this.partitionName = partitionName;
   }
   /**
    * Gets the loadBalancePolicy
    * @return Returns a String
    */
   public String getLoadBalancePolicy() {
      return loadBalancePolicy;
   }
   /**
    * Sets the loadBalancePolicy
    * @param loadBalancePolicy The loadBalancePolicy to set
    */
   public void setLoadBalancePolicy(String loadBalancePolicy) {
      this.loadBalancePolicy = loadBalancePolicy;
   }

}
