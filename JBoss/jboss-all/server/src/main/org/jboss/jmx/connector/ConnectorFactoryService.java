/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.jmx.connector;

import java.util.Iterator;
import java.util.Hashtable;

import javax.management.DynamicMBean;
import javax.management.ObjectName;
import javax.management.MBeanServer;

import javax.naming.InitialContext;

import org.jboss.jmx.connector.RemoteMBeanServer;
import org.jboss.system.ServiceMBeanSupport;

/**
 * Factory delivering a list of servers and its available protocol connectors
 * and after selected to initiate the connection
 *
 * @jmx:mbean name="jboss.rmi.connector:name=JMX"
 *            extends="org.jboss.system.ServiceMBean"
 *
 * @version <tt>$Revision: 1.8 $</tt>
 * @author  <A href="mailto:andreas.schaefer@madplanet.com">Andreas &quot;Mad&quot; Schaefer</A>
 */
public class ConnectorFactoryService
   extends ServiceMBeanSupport
   implements ConnectorFactoryServiceMBean
{
   private static final String JNDI_NAME = "jxm:connector:factory";

   /** Connector Factory instance **/
   private ConnectorFactoryImpl	mFactory;
   private int mNotificationType;
   private String mJMSName;
   private String mEJBAdaptorName;

   public ConnectorFactoryService() {
      super();
   }

   /**
    * @jmx:managed-attribute
    */
   public int getNotificationType() {
      return mNotificationType;
   }
   
   /**
    * @jmx:managed-attribute
    */
   public void setNotificationType( int pNotificationType ) {
      mNotificationType = pNotificationType;
   }
   
   /**
    * @jmx:managed-attribute
    * 
    * @return JMS Queue Name and if not null then JMS will be used
    **/
   public String getJMSName() {
      return mJMSName;
   }

   /**
    * Sets the JMS Queue Factory Name which allows the server to send
    * the notifications asynchronous to the client
    *
    * @jmx:managed-attribute
    *
    * @param pName If null the notification will be transferred
    *              by using RMI Callback objects otherwise it
    *              will use JMS
    **/   
   public void setJMSName( String pName ) {
      mJMSName = pName;
   }

   /**
    * @jmx:managed-attribute
    * 
    * @return EJB Adaptor JNDI Name used by the EJB-Connector
    **/
   public String getEJBAdaptorName() {
      return mEJBAdaptorName;
   }
   
   /**
    * Sets the JNDI Name of the EJB-Adaptor
    *
    * @jmx:managed-attribute
    *
    * @param pName If null the default JNDI name (ejb/jmx/ejb/adaptor) will
    *              be used for EJB-Connectors otherwise it will use this one
    **/
   public void setEJBAdaptorName( String pName ) {
      if( pName == null ) {
         mEJBAdaptorName = "ejb/jmx/ejb/adaptor";
      } else {
         mEJBAdaptorName = pName;
      }
   }

   /**
    * Look up for all registered JMX Connector at a given JNDI server
    *
    * @jmx:managed-operation
    *
    * @param pProperties List of properties defining the JNDI server
    * @param pTester Connector Tester implementation to be used
    *
    * @return An iterator on the list of ConnectorNames representing
    *         the found JMX Connectors
    **/
   public Iterator getConnectors( Hashtable pProperties, ConnectorFactoryImpl.IConnectorTester pTester )
   {
      return mFactory.getConnectors( pProperties, pTester );
   }

   /**
    * Initiate a connection to the given server with the given protocol
    *
    * @jmx:managed-operation
    *
    * @param pConnector Connector Name used to identify the remote JMX Connector
    *
    * @return JMX Connector or null if server or protocol is not supported
    **/
   public RemoteMBeanServer createConnection(ConnectorFactoryImpl.ConnectorName pConnector)
   {
      return mFactory.createConnection( pConnector );
   }

   /**
    * Removes the given connection and frees the resources
    *
    * @jmx:managed-operation
    *
    * @param pConnector Connector Name used to identify the remote JMX Connector
    **/
   public void removeConnection(ConnectorFactoryImpl.ConnectorName pConnector)
   {
      mFactory.removeConnection( pConnector );
   }

   protected ObjectName getObjectName(MBeanServer pServer, ObjectName pName)
      throws javax.management.MalformedObjectNameException
   {
      return pName == null ? OBJECT_NAME : pName;
   }
	
   protected void startService() throws Exception 
   {
      log.debug( "Init Connector Factory mNotificationTypeService: " +
                 "NT: " + mNotificationType + ", JMS: " + mJMSName
                 );
      mFactory = new ConnectorFactoryImpl( server, mNotificationType, mJMSName, mEJBAdaptorName );
   }
}
