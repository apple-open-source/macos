/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.jmx.connector;

import java.util.Arrays;
import java.util.Collection;
import java.util.Hashtable;
import java.util.Iterator;
import java.util.Set;
import java.util.StringTokenizer;
import java.util.Vector;

import javax.management.DynamicMBean;
import javax.management.MBeanServer;
import javax.management.ObjectInstance;
import javax.management.ObjectName;

import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NameClassPair;
import javax.naming.NamingEnumeration;
import javax.naming.NamingException;

import org.jboss.jmx.connector.RemoteMBeanServer;
import org.jboss.jmx.connector.ejb.EJBConnector;
import org.jboss.jmx.connector.rmi.RMIConnectorImpl;

import org.jboss.logging.Logger;

/**
 * Factory delivering a list of servers and its available protocol connectors
 * and after selected to initiate the connection This is just the (incomplete)
 * interface of it
 *
 * @created  May 2, 2001
 * @version <tt>$Revision: 1.6 $</tt>
 * @author  <A href="mailto:andreas@jboss.org">Andreas Schaefer</A>
 **/
public class ConnectorFactoryImpl
{
   private static final Logger log = Logger.getLogger(ConnectorFactoryImpl.class);

   private MBeanServer mServer;
   private int mNotificationType = RemoteMBeanServer.NOTIFICATION_TYPE_RMI;
   private String mJMSName;
   private String mEJBAdaptorName = "ejb/jmx/ejb/Adaptor";

   public ConnectorFactoryImpl(
      MBeanServer pServer,
      int pNotificationType
   ) {
      this( pServer, pNotificationType, null, null );
   }

   public ConnectorFactoryImpl(
      MBeanServer pServer,
      int pNotificationType,
      String pJMSQueueName
   ) {
      this( pServer, pNotificationType, pJMSQueueName, "ejb/jmx/ejb/adaptor" );
   }

   public ConnectorFactoryImpl(
      MBeanServer pServer,
      int pNotificationType,
      String pJMSName,
      String pEJBAdaptorName
   ) {
      mServer = pServer;
      if( pJMSName != null ) {
         mNotificationType = RemoteMBeanServer.NOTIFICATION_TYPE_JMS;
         mJMSName = pJMSName;
      } else {
         mNotificationType = pNotificationType;
      }
      if( pEJBAdaptorName != null && pEJBAdaptorName.trim().length() > 0 ) {
         mEJBAdaptorName = pEJBAdaptorName;
      }
   }

   /**
    * Look up for all registered JMX Connector at a given JNDI server
    *
    * @param pProperties List of properties defining the JNDI server
    * @param pTester Connector Tester implementation to be used
    *
    * @return An iterator on the list of ConnectorNames representing
    *         the found JMX Connectors
    **/
   public Iterator getConnectors( Hashtable pProperties, IConnectorTester pTester ) {
      Vector lConnectors = new Vector();
      try {
         InitialContext lNamingServer = new InitialContext( pProperties );
         // AS Check if the lItem.getName() (below) is the right server name or if the
         // lJNDIServer should be used
         String lJNDIServer = (String) lNamingServer.getEnvironment().get( Context.PROVIDER_URL );
         // Lookup the JNDI server
         NamingEnumeration enum = lNamingServer.list( "" );
         while( enum.hasMore() ) {
            NameClassPair lItem = ( NameClassPair ) enum.next();
            ConnectorName lName = pTester.check( lNamingServer, lItem );
            if( lName != null ) {
               lConnectors.add( lName );
            }
         }

         lNamingServer.close();
      }
      catch( Exception e ) {
         //
         // jason: should probably declare this exception and let cleint handle
         //
         log.error("operation failed", e);
      }

      return lConnectors.iterator();
   }

   /**
    * Initiate a connection to the given server with the given protocol
    *
    * @param pConnector Connector Name used to identify the remote JMX Connector
    *
    * @return JMX Connector or null if server or protocol is not supported
    **/
   public RemoteMBeanServer createConnection(
      ConnectorName pConnector
   ) {
      RemoteMBeanServer lConnector = null;
      // At the moment only RMI and EJB protocol is supported (on the client side)
      if( pConnector.getProtocol().equals( "rmi" ) ) {
         try {
            lConnector = new RMIConnectorImpl(
               mNotificationType,
               new String[] { mJMSName },
               pConnector.getServer()
            );
            mServer.registerMBean(
               lConnector,
               new ObjectName( "jboss:name=RMIConnectorTo" + pConnector.getServer() )
            );
         }
         catch( Exception e ) {
            //
            // jason: should probably declare this exception and let cleint handle
            //
            log.error("operation failed", e);
         }
      }
      else if( pConnector.getProtocol().equals( "ejb" ) ) {
         try {
            lConnector = new EJBConnector(
               mNotificationType,
               new String[] { mJMSName },
               pConnector.getJNDIName(),
               pConnector.getServer()
            );
            mServer.registerMBean(
               lConnector,
               new ObjectName( "jboss:name=EJBConnectorTo" + pConnector.getServer() )
            );
         }
         catch( Exception e ) {
            //
            // jason: should probably declare this exception and let cleint handle
            //
            log.error("operation failed", e);
         }
      }
      return lConnector;
   }

   /**
    * Removes the given connection and frees the resources
    *
    * @param pConnector Connector Name used to identify the remote JMX Connector
    **/
   public void removeConnection(
      ConnectorName pConnector
   ) {
      try {
         if( pConnector.getProtocol().equals( "rmi" ) ) {
            Set lConnectors = mServer.queryMBeans(
               new ObjectName( "jboss:name=RMIConnectorTo" + pConnector.getServer() ),
               null
            );
            if( !lConnectors.isEmpty() ) {
               Iterator i = lConnectors.iterator();
               while( i.hasNext() ) {
                  ObjectInstance lConnector = ( ObjectInstance ) i.next();
                  mServer.invoke(
                     lConnector.getObjectName(),
                     "stop",
                     new Object[] {},
                     new String[] {}
                  );
                  mServer.unregisterMBean(
                     lConnector.getObjectName()
                  );
               }
            }
         }
         else if( pConnector.getProtocol().equals( "ejb" ) ) {
            Set lConnectors = mServer.queryMBeans(
               new ObjectName( "jboss:name=EJBConnectorTo" + pConnector.getServer() ),
               null
            );
            if( !lConnectors.isEmpty() ) {
               Iterator i = lConnectors.iterator();
               while( i.hasNext() ) {
                  ObjectInstance lConnector = ( ObjectInstance ) i.next();
                  mServer.invoke(
                     lConnector.getObjectName(),
                     "stop",
                     new Object[] {},
                     new String[] {}
                  );
                  mServer.unregisterMBean(
                     lConnector.getObjectName()
                  );
               }
            }
         }
      }
      catch( Exception e ) {
         //
         // jason: should probably declare this exception and let cleint handle
         //
         log.error("operation failed", e);
      }
   }

   /**
    * Interface defined a Connector Tester to verify JMX Connectors
    * based on the information delivered by a JNDI server
    *
    * @author Andreas Schaefer (andreas.schaefer@madplanet.com)
    **/
   public static interface IConnectorTester {
      
      /**
       * Checks a given JNDI entry if it is a valid JMX Connector
       *
       * @param pName JNDI Name of the entry to test for
       * @param pClass Class of the entry
       *
       * @return Connector Name instance if valid otherwise null
       **/
      ConnectorName check( Context pContext, NameClassPair pPair );
      
   }

   /**
    * Default implementation of the jBoss JMX Connector tester
    *
    * @author Andreas Schaefer (andreas.schaefer@madplanet.com)
    **/
   public static class JBossConnectorTester
      implements IConnectorTester
   {
      
      public ConnectorName check( Context pContext, NameClassPair pPair ) {
         ConnectorName lConnector = null;
         if( pPair != null ) {
            String lName = pPair.getName();
            if( lName.startsWith( "jmx:" ) ) {
               // Server-side Connector registered as MBean
               StringTokenizer lTokens = new StringTokenizer( lName, ":" );
               if( lTokens.hasMoreTokens() && lTokens.nextToken().equals( "jmx" ) ) {
                  if( lTokens.hasMoreTokens() ) {
                     String lServer = lTokens.nextToken();
                     if( lTokens.hasMoreTokens() ) {
                        lConnector = new ConnectorName( lServer, lTokens.nextToken(), lName );
                     }
                  }
               }
            }
            if( lName.equals( "ejb" ) ) {
               try {
                  Context lContext = (Context) pContext.lookup( "ejb" );
                  lContext = (Context) lContext.lookup( "jmx" );
                  lContext = (Context) lContext.lookup( "ejb" );
                  if( lContext.lookup( "Adaptor" ) != null ) {
                     lConnector = new ConnectorName( "" + pContext.getEnvironment().get( Context.PROVIDER_URL ), "ejb", "ejb/jmx/ejb/Adaptor" );
                  }
               }
               catch( NamingException ne ) {
               }
            }
         }
         return lConnector;
      }
      
   }

   /**
    * Container for a JMX Connector representation
    *
    * @author Andreas Schaefer (andreas.schaefer@madplanet.com)
    **/
   public static class ConnectorName {
      
      private String mServer;
      private String mProtocol;
      private String mJNDIName;
      
      /**
       * Creates a Connector Name instance
       *
       * @param pServer Name of the Server the JMX Connector is registered at
       * @param pProtocol Name of the Protocol the JMX Connector supports
       * @param pJNDIName JNDI Name the JMX Connector can be found
       **/
      public ConnectorName( String pServer, String pProtocol, String pJNDIName ) {
         mServer = pServer;
         mProtocol = pProtocol;
         mJNDIName = pJNDIName;
      }
      
      /**
       * @return Name of the Server the JMX Connector is registered at
       **/
      public String getServer() {
         return mServer;
      }
      
      /**
       * @return Name of the Protocol the JMX Connector supports
       **/
      public String getProtocol() {
         return mProtocol;
      }
      
      /**
       * @return JNDI Name the JMX Connector can be found
       **/
      public String getJNDIName() {
         return mJNDIName;
      }
      
      /**
       * @return Debug information about this instance
       **/
      public String toString() {
         return "ConnectorName [ server: " + mServer +
            ", protocol: " + mProtocol +
            ", JNDI name: " + mJNDIName + " ]";
      }
   }

}
