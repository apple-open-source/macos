/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.cluster;

import java.rmi.RemoteException;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

import javax.management.Attribute;
import javax.management.AttributeList;
import javax.management.AttributeNotFoundException;
import javax.management.InvalidAttributeValueException;
import javax.management.InstanceNotFoundException;
import javax.management.InstanceAlreadyExistsException;
import javax.management.MBeanException;
import javax.management.MBeanInfo;
import javax.management.MBeanRegistrationException;
import javax.management.MBeanServer;
import javax.management.IntrospectionException;
import javax.management.ListenerNotFoundException;
import javax.management.NotCompliantMBeanException;
import javax.management.NotificationFilter;
import javax.management.ObjectName;
import javax.management.ObjectInstance;
import javax.management.QueryExp;
import javax.management.ReflectionException;

import org.jboss.ha.framework.interfaces.HAPartition;
import org.jboss.management.j2ee.J2EEDomain;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.mx.util.ObjectNameFactory;

/**
 * This class enables a client to get a management view (JSR-77) on a
 * JBoss Cluster. It contains the entire logic to map all the {@link
 * org.jboss.management.j2ee.J2EEManagedObject Managed Object (MO) of
 * all the nodes in the cluster to one MO. The same applies to State
 * Management and Performance Statistics.
 *
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.2 $ <p>
 *
 * @jmx:mbean extends="org.jboss.system.ServiceMBean"
 **/
public class HAManagementService
   extends ServiceMBeanSupport
   implements HAManagementServiceMBean
{
   
   // Constants -----------------------------------------------------
   
   private final static String SERVICE_NAME = "HAManagementService";
   /** The default object name. */
   ObjectName OBJECT_NAME = ObjectNameFactory.create( "jboss:service=HAManagement" );
   
   // Attributes ----------------------------------------------------
   
   private ObjectName mHAManagementName;
   private ObjectName mClusterPartitionName;
   private String mBackgroundPartition = "DefaultPartition";
   private HAPartition mPartition;
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   /**
   * Creates a undefined clustering management service. Before this
   * MBean can be started the HAPartition must be up and running.
   *
   * ATTENTION: The current implemenation is a brute force one because
   * it is retrieving the data on every hit. Later on this has to be
   * resolved with a caching to avoid unecessary round-trips.
   **/
   public HAManagementService() {
   }
   
   // Public --------------------------------------------------------
   
   /**
    **/
   public Object _getAttribute( ObjectName pName, String pAttribute )
   {
      Object lReturn = null;
      try {
         lReturn = server.getAttribute( pName, pAttribute );
      }
      catch( Exception e ) {
         // Ignore them
      }
      return lReturn;
   }
   
   /**
    * @jmx:managed-attribute
    **/
   public Object getAttribute( ObjectName pName, String pAttribute )
      throws
         MBeanException,
         AttributeNotFoundException,
         InstanceNotFoundException,
         ReflectionException,
         RemoteException
   {
      // First we try to get the attribute from the local node
      // and if not found the look for an instance out in the
      // node. This does not apply for JVMs.
      //AS ToDo: JVMs are not supported yet
      Object lReturn = null;
      try {
         lReturn = server.getAttribute( pName, pAttribute );
      }
      catch( InstanceNotFoundException infe ) {
         // Ignore and search on the cluster
         Object[] lArguments = new Object[] {
            pName,
            pAttribute
         };
         List lValues = null;
         try {
            lValues = mPartition.callMethodOnCluster(
               SERVICE_NAME,
               "_getAttribute",
               lArguments,
               false
            );
         }
         catch( Exception e ) {
            throw new RemoteException( "Could not get management attributes on the cluster", e );
         }
         Iterator i = lValues.iterator();
         while( i.hasNext() ) {
            Object lValue = i.next();
            if( lValue != null ) {
               lReturn = lValue;
               break;
            }
         }
         if( lReturn == null ) {
            // Throw the instance not found exception because there is none
            throw infe;
         }
      }
      return lReturn;
   }
   
   /**
    **/
   public AttributeList _getAttributes( ObjectName pName, String[] pAttributes ) {
      AttributeList lReturn = null;
      try {
         lReturn = server.getAttributes( pName, pAttributes );
      }
      catch( Exception e ) {
         // Ignore them
      }
      return lReturn;
   }
   
   /**
    * @jmx:managed-attribute
    **/
   public AttributeList getAttributes( ObjectName pName, String[] pAttributes )
      throws
         InstanceNotFoundException,
         ReflectionException,
         RemoteException
   {
      // First we try to get the attribute from the local node
      // and if not found the look for an instance out in the
      // node. This does not apply for JVMs.
      //AS ToDo: JVMs are not supported yet
      //AS ToDo: Now only the first list is taken, shall we add a merge capabilities ?
      AttributeList lReturn = null;
      try {
         lReturn = server.getAttributes( pName, pAttributes );
      }
      catch( InstanceNotFoundException infe ) {
         // Ignore and search on the cluster
         Object[] lArguments = new Object[] {
            pName,
            pAttributes
         };
         List lValues = null;
         try {
            lValues = mPartition.callMethodOnCluster(
               SERVICE_NAME,
               "_getAttributes",
               lArguments,
               false
            );
         }
         catch( Exception e ) {
            throw new RemoteException( "Could not get management attributes on the cluster", e );
         }
         Iterator i = lValues.iterator();
         while( i.hasNext() ) {
            Object lValue = i.next();
            if( lValue != null ) {
               lReturn = (AttributeList) lValue;
               break;
            }
         }
         if( lReturn == null ) {
            // Throw the instance not found exception because there is none
            throw infe;
         }
      }
      return lReturn;
   }
   
   /**
    * @jmx:managed-attribute
    **/
   public String getDefaultDomain()
      throws RemoteException
   {
      return J2EEDomain.getDomainName();
   }
   
   /**
    * @jmx:managed-attribute
    **/
   public Integer getMBeanCount()
      throws RemoteException
   {
      try {
         return new Integer(
            queryNames(
               new ObjectName( getDefaultDomain() + ":*" ),
               null
            ).size()
         );
      }
      catch( Exception e ) {
      }
      return new Integer( 0 );
   }
   
   /**
    * @jmx:managed-attribute
    **/
   public MBeanInfo getMBeanInfo( ObjectName pName )
      throws
         IntrospectionException,
         InstanceNotFoundException,
         ReflectionException,
         RemoteException
   {
      return server.getMBeanInfo( pName );
   }
   
   /**
    * @jmx:managed-attribute
    **/
   public javax.management.j2ee.ListenerRegistration getListenerRegistry()
      throws RemoteException
   {
      return null;
/*
      return new ListenerRegistration(
         (ManagementHome) mContext.getEJBObject().getEJBHome(),
         new String[] {}
      );
*/
   }
   
   /**
    **/
   public Object _invoke( ObjectName pName, String pOperationName, Object[] pParams, String[] pSignature ) {
      Object lReturn = null;
      try {
         log.info( "_invoke(), name: " + pName + ", operation: " + pOperationName +
            ", params: " + pParams + ", signature: " + pSignature );
         lReturn = server.invoke(
            pName,
            pOperationName,
            pParams,
            pSignature
         );
      }
      catch( Exception e ) {
         // Return the exception instead
         lReturn = e;
      }
      return lReturn;
   }
   
   /**
    * @jmx:managed-attribute
    **/
   public Object invoke( ObjectName pName, String pOperationName, Object[] pParams, String[] pSignature )
      throws
         InstanceNotFoundException,
         MBeanException,
         ReflectionException,
         RemoteException
   {
      Object lReturn = null;
      InstanceNotFoundException lException = null;
      log.info( "invoke(), name: " + pName + ", operation: " + pOperationName +
         ", params: " + pParams + ", signature: " + pSignature );
      try {
         lReturn = server.invoke( pName, pOperationName, pParams, pSignature );
      }
      catch( InstanceNotFoundException infe ) {
         lException = infe;
      }
      Object[] lArguments = new Object[] {
         pName,
         pOperationName,
         pParams,
         pSignature
      };
      List lValues = null;
      try {
         log.info( "call _invoke()" );
         lValues = mPartition.callMethodOnCluster(
            SERVICE_NAME,
            "_invoke",
            lArguments,
            true
         );
      }
      catch( Exception e ) {
         throw new RemoteException( "Could not get management attributes on the cluster", e );
      }
      Iterator i = lValues.iterator();
      while( i.hasNext() ) {
         Object lValue = i.next();
         if( lValue instanceof Throwable ) {
            log.debug( "invoke a method on the cluster caused an exception: " + lValue );
            if( lValue instanceof InstanceNotFoundException ) {
               // Go ahead when INFE is found
               continue;
            }
         }
         // A non-INFE is found therefore ignore all the other INFE
         lException = null;
         if( lValue != null ) {
            lReturn = lValue;
            break;
         }
      }
      if( lException != null ) {
         // Only if all calls throws an INFE it will throw this exception
         throw lException;
      }
      return lReturn;
   }
   
   /**
    * @jmx:managed-attribute
    **/
   public boolean isRegistered( ObjectName pName )
      throws RemoteException
   {
      return server.isRegistered( pName );
   }
   
   /**
    * @jmx:managed-attribute
    **/
   public Set queryNames( ObjectName pName, QueryExp pQuery )
      throws RemoteException
   {
      return server.queryNames( pName, pQuery );
   }
   
   /**
    **/
   public Object _setAttribute( ObjectName pName, Attribute pAttribute ) {
      Object lReturn = null;
      try {
         log.info( "_setAttribute(), name: " + pName + ", attribute: " + pAttribute );
         server.setAttribute( pName, pAttribute );
      }
      catch( Exception e ) {
         // Return the exception instead
         lReturn = e;
      }
      return lReturn;
   }
   
   /**
    * @jmx:managed-attribute
    **/
   public void setAttribute( ObjectName pName, Attribute pAttribute )
      throws
         AttributeNotFoundException,
         InstanceNotFoundException,
         InvalidAttributeValueException,
         MBeanException,
         ReflectionException,
         RemoteException
   {
      InstanceNotFoundException lException = null;
      try {
         server.setAttribute( pName, pAttribute );
      }
      catch( InstanceNotFoundException infe ) {
         lException = infe;   // Remember that instance was not found
      }
      Object[] lArguments = new Object[] {
         pName,
         pAttribute
      };
      List lValues = null;
      try {
         log.info( "call _setAttribute()" );
         lValues = mPartition.callMethodOnCluster(
            SERVICE_NAME,
            "_setAttribute",
            lArguments,
            true
         );
      }
      catch( Exception e ) {
         throw new RemoteException( "Could not set management attributes on the cluster", e );
      }
      Iterator i = lValues.iterator();
      while( i.hasNext() ) {
         Object lValue = i.next();
         // If one value is null (because the method does not return a value) then
         // everything is fine
         if( lValue instanceof Throwable ) {
            log.debug( "invoke a method on the cluster caused an exception: " + lValue );
            if( lValue instanceof InstanceNotFoundException ) {
               if( lException == null ) {
                  lException = (InstanceNotFoundException) lValue; // Remember this exception
               }
            } else {
               // Only Throwables are returned
               if( lValue instanceof AttributeNotFoundException ) {
                  throw (AttributeNotFoundException) lValue;
               }
               if( lValue instanceof InvalidAttributeValueException ) {
                  throw (InvalidAttributeValueException) lValue;
               }
               if( lValue instanceof MBeanException ) {
                  throw (MBeanException) lValue;
               }
               if( lValue instanceof ReflectionException ) {
                  throw (ReflectionException) lValue;
               }
               throw new RemoteException( lValue.toString() );
            }
         }
      }
      if( lException != null ) {
         throw lException; // Now this exception can be thrown because it has low priority
      }
   }
   
   /**
    **/
   public Object _setAttributes( ObjectName pName, AttributeList pAttributes ) {
      Object lReturn = null;
      try {
         log.info( "_setAttributes(), name: " + pName + ", attribute: " + pAttributes );
         server.setAttributes( pName, pAttributes );
      }
      catch( Exception e ) {
         // Return the exception instead
         lReturn = e;
      }
      return lReturn;
   }
   
   /**
    * @jmx:managed-attribute
    **/
   public AttributeList setAttributes( ObjectName pName, AttributeList pAttributes )
      throws
         InstanceNotFoundException,
         ReflectionException,
         RemoteException
   {
      Object lReturn = null;
      InstanceNotFoundException lException = null;
      try {
         lReturn = server.setAttributes( pName, pAttributes );
      }
      catch( InstanceNotFoundException infe ) {
         lException = infe;
      }
      Object[] lArguments = new Object[] {
         pName,
         pAttributes
      };
      List lValues = null;
      try {
         log.info( "call _setAttributes()" );
         lValues = mPartition.callMethodOnCluster(
            SERVICE_NAME,
            "_setAttributes",
            lArguments,
            true
         );
      }
      catch( Exception e ) {
         throw new RemoteException( "Could not set management attributes on the cluster", e );
      }
      Iterator i = lValues.iterator();
      while( i.hasNext() ) {
         Object lValue = i.next();
         // If one value is null (because the method does not return a value) then
         // everything is fine
         if( lValue instanceof Throwable ) {
            log.debug( "set Attributes on the cluster caused an exception: " + lValue );
            if( lValue instanceof InstanceNotFoundException ) {
               if( lException == null ) {
                  lException = (InstanceNotFoundException) lValue; // Remember this exception
               }
            } else {
               if( lValue instanceof ReflectionException ) {
                  throw (ReflectionException) lValue;
               }
               throw new RemoteException( lValue.toString() );
            }
         }
      }
      if( lException != null ) {
         // If no other exception is thrown then the INFE will be thrown here
         throw lException;
      }
      return (AttributeList) lReturn;
   }
   
   /**
    **/
   public Object _createMBean(
      String pClass,
      ObjectName pName,
      Object[] pParameters,
      String[] pSignature
   ) {
      Object lReturn = null;
      try {
         log.info( "_createMBean(), name: " + pName );
         lReturn = server.createMBean( pClass, pName, pParameters, pSignature );
      }
      catch( Exception e ) {
         // Return the exception instead
         lReturn = e;
      }
      return lReturn;
   }
   
   /**
    * @jmx:managed-attribute
    **/
   public ObjectInstance createMBean(
      String pClass,
      ObjectName pName,
      Object[] pParameters,
      String[] pSignature
   )
      throws
         InstanceAlreadyExistsException,
         MBeanException,
         MBeanRegistrationException,
         NotCompliantMBeanException,
         ReflectionException,
         RemoteException
   {
      List lValues = null;
      Object[] lArguments = new Object[] {
         pClass,
         pName,
         pParameters,
         pSignature
      };
      try {
         log.info( "call _createMBean()" );
         lValues = mPartition.callMethodOnCluster(
            SERVICE_NAME,
            "_createMBean",
            lArguments,
            false
         );
      }
      catch( Exception e ) {
         //AS ToDo: must be checked later how to go ahead here
         throw new RemoteException( "Could not create a MBean on the cluster", e );
      }
      Iterator i = lValues.iterator();
      ObjectInstance lInstance = null;
      Throwable lException = null;
      while( i.hasNext() ) {
         Object lValue = i.next();
         if( lValue instanceof ObjectInstance ) {
            if( lInstance == null ) {
               lInstance = (ObjectInstance) lValue;
            }
         } else
         if( lValue instanceof Throwable ) {
            if( lException == null ) {
               lException = (Throwable) lValue;
            }
         }
      }
      if( lException != null ) {
         if( lInstance != null ) {
            // Remove all existing MBeans
            try {
               unregisterMBean( lInstance.getObjectName() );
            }
            catch( Exception e ) {
               // Ignore any exception
            }
         }
         if( lException instanceof InstanceAlreadyExistsException ) {
            throw (InstanceAlreadyExistsException) lException;
         }
         if( lException instanceof MBeanException ) {
            throw (MBeanException) lException;
         }
         if( lException instanceof MBeanRegistrationException ) {
            throw (MBeanRegistrationException) lException;
         }
         if( lException instanceof NotCompliantMBeanException ) {
            throw (NotCompliantMBeanException) lException;
         }
         if( lException instanceof ReflectionException ) {
            throw (ReflectionException) lException;
         }
         throw new RemoteException( lException.toString() );
      }
      return lInstance;
   }
   
   /**
    **/
   public Object _unregisterMBean(
      ObjectName pName
   ) {
      Object lReturn = null;
      try {
         log.info( "_unregisterMBean(), name: " + pName );
         server.unregisterMBean( pName );
      }
      catch( Exception e ) {
         // Return the exception instead
         lReturn = e;
      }
      return lReturn;
   }
   
   /**
    * @jmx:managed-attribute
    **/
   public void unregisterMBean(
      ObjectName pName
   )
      throws
         InstanceNotFoundException,
         MBeanRegistrationException,
         RemoteException
   {
      List lValues = null;
      Object[] lArguments = new Object[] {
         pName
      };
      try {
         log.info( "call _unregisterMBean()" );
         lValues = mPartition.callMethodOnCluster(
            SERVICE_NAME,
            "_unregisterMBean",
            lArguments,
            false
         );
      }
      catch( Exception e ) {
         //AS ToDo: must be checked later how to go ahead here
         throw new RemoteException( "Could not unregister a MBean on the cluster", e );
      }
      Iterator i = lValues.iterator();
      Throwable lException = null;
      while( i.hasNext() ) {
         Object lValue = i.next();
         if( lValue instanceof Throwable ) {
            lException = (Throwable) lValue;
            break;
         }
      }
      if( lException != null ) {
         if( lException instanceof InstanceNotFoundException ) {
            throw (InstanceNotFoundException) lException;
         }
         if( lException instanceof MBeanRegistrationException ) {
            throw (MBeanRegistrationException) lException;
         }
         throw new RemoteException( lException.toString() );
      }
   }
   
   /**
    * @jmx:managed-attribute
    **/
   public void addNotificationListener(
      ObjectName pBroadcaster,
      ObjectName pListener,
      NotificationFilter pFilter,
      Object pHandback
   )
      throws
         InstanceNotFoundException,
         RemoteException
   {
      server.addNotificationListener( pBroadcaster, pListener, pFilter, pHandback );
   }
   
   /**
    * @jmx:managed-attribute
    **/
   public void removeNotificationListener(
      ObjectName pBroadcaster,
      ObjectName pListener
   )
      throws
         InstanceNotFoundException,
         ListenerNotFoundException,
         RemoteException
   {
      server.removeNotificationListener( pBroadcaster, pListener );
   }
   
   // MBeanRegistration implementation ----------------------------------------
   
   /** Saves the MBeanServer reference
    */
   public ObjectName preRegister( MBeanServer pServer, ObjectName pName )
      throws Exception
   {
      super.preRegister( pServer, pName );
      log.info( "HA Management Service MBean online" );
      mHAManagementName = new ObjectName(
         OBJECT_NAME + ",Partition=" + mBackgroundPartition
      );
      
      return mHAManagementName;
   }

   /**
    * Removes the Notification Listener
    **/
   public void preDeregister()
      throws Exception
   {
      super.preDeregister();
   }
   
   // Service implementation ----------------------------------------

   public String getName()
   {
      return "HA Management Service";
   }
   
   /**
    * Looks up the Server Config instance to figure out the
    * temp-directory and the farm-deploy-directory
    **/
   protected void createService() throws Exception
   {
   }
   
   /**
    * Register itself as RPC-Handler to the HA-Partition
    * and add the farm deployment directory to the scanner
    **/
   protected void startService()
      throws Exception
   {
      mClusterPartitionName = new ObjectName( "jboss:service=" + mBackgroundPartition );
      
      log.debug( "registerRPCHandler" );
      mPartition = (HAPartition) server.getAttribute(
         mClusterPartitionName,
         "HAPartition"
      );
      mPartition.registerRPCHandler( SERVICE_NAME, this );
   }
   
   /**
    * Remove the farm deployment directory from the scanner
    **/
   protected void stopService()
   {
   }

   // Protected -----------------------------------------------------

   /**
    * Go through the myriad of nested JMX exception to pull out the true
    * exception if possible and log it.
    *
    * @param e The exception to be logged.
    */
   private void logException( Throwable e )
   {
      Throwable t = org.jboss.mx.util.JMXExceptionDecoder.decode(e);
      log.error("operation failed", t);
   }

   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
}
