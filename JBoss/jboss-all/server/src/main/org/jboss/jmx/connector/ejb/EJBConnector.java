/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.jmx.connector.ejb;

import org.jboss.jmx.adaptor.interfaces.Adaptor;
import org.jboss.jmx.adaptor.interfaces.AdaptorHome;

import java.util.Hashtable;

import java.rmi.RemoteException;

import javax.ejb.CreateException;

import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;

import javax.rmi.PortableRemoteObject;

import org.jboss.jmx.connector.RemoteMBeanServer;

import org.jboss.jmx.connector.rmi.RMIConnectorImpl;

import org.jboss.util.NestedRuntimeException;

/**
 * This is the equivalent to the RMI Connector but uses the
 * EJB Adaptor. The only advantage of using EJB-Adaptor is
 * that you can utilize the security.
 *
 * <p>
 * <b>ATTENTION</b>: Note that for the event transport (or in the
 * JMX Notations: Notification) the server must be able to
 * load the RMI Stubs and the Remote Listener classes.
 * Therefore you must make them available to the JMX Server
 * and the EJB-Adaptor (meaning the EJB-Container).
 *
 * <p>
 * Translates RemoteExceptions into MBeanExceptions where declared and
 * RuntimeMBeanExceptions when not declared. RuntimeMBeanException contain
 * NestedRuntimeException containg the root RemoteException due to
 * RuntimeMBeanException taking a RuntimeException and not a Throwable for
 * detail.
 *
 * @jmx:mbean extends "org.jboss.jmx.connector.RemoteMBeanServer"
 *
 * @version <tt>$Revision: 1.10 $</tt>
 * @author  Andreas Schaefer (andreas.schaefer@madplanet.com)
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 **/
public class EJBConnector
   extends RMIConnectorImpl
   implements RemoteMBeanServer, EJBConnectorMBean
{
   protected String mJNDIServer;
   protected String mJNDIName;

   /**
    * AS For evaluation purposes
    * Creates a Connector based on an already found Adaptor
    *
    * @param pAdaptor RMI-Adaptor used to connect to the remote JMX Agent
    **/
   public EJBConnector(AdaptorHome pAdaptorHome)
      throws CreateException, RemoteException
   {
      mRemoteAdaptor = pAdaptorHome.create();
   }

   /**
    * Creates an Client Connector using the EJB-Adaptor to
    * access the remote JMX Server.
    *
    * @param pType Defines the type of event transport. Please have a
    *              look at the constants with the prefix NOTIFICATION_TYPE
    *              which protocols are supported
    * @param pOptions List of options used for the event transport. Right
    *                 now only for event type JMS there is the JMS Queue-
    *                 Factory JNDI Name supported.
    **/
   public EJBConnector( int pType, String[] pOptions )
      throws Exception
   {
      this( pType, pOptions, null, null );
   }
   
   /**
    * Creates an Client Connector using the EJB-Adaptor to
    * access the remote JMX Server.
    *
    * @param pType Defines the type of event transport. Please have a
    *              look at the constants with the prefix NOTIFICATION_TYPE
    *              which protocols are supported
    * @param pOptions List of options used for the event transport. Right
    *                 now only for event type JMS there is the JMS Queue-
    *                 Factory JNDI Name supported.
    * @param pJNDIName JNDI Name of the EJB-Adaptor to lookup its Home interface
    *                  and if null then "ejb/jmx/ejb/Adaptor" is used as default
    **/
   public EJBConnector( int pType, String[] pOptions, String pJNDIName )
      throws Exception
   {
      this( pType, pOptions, pJNDIName, null );
   }
   
   /**
    * Creates an Client Connector using the EJB-Adaptor to
    * access the remote JMX Server.
    *
    * @param pType Defines the type of event transport. Please have a
    *              look at the constants with the prefix NOTIFICATION_TYPE
    *              which protocols are supported
    * @param pOptions List of options used for the event transport. Right
    *                 now only for event type JMS there is the JMS Queue-
    *                 Factory JNDI Name supported.
    * @param pJNDIName JNDI Name of the EJB-Adaptor to lookup its Home interface
    *                  and if null then "ejb/jmx/ejb/Adaptor" is used as default
    * @param pJNDIServer Server name of the JNDI server to look up the EJB-Adaptor
    *                    and QueueFactory if JMS is used for the event transport.
    *                    If null then the default specified in the "jndi.properties"
    *                    will be used.
    **/
   public EJBConnector( int pType, String[] pOptions, String pJNDIName, String pJNDIServer )
      throws Exception
   {
      if( pType == NOTIFICATION_TYPE_RMI || pType == NOTIFICATION_TYPE_JMS
         || pType == NOTIFICATION_TYPE_POLLING ) {
         mEventType = pType;
      }
      if( pOptions != null ) {
         mOptions = pOptions;
      }
      if( pJNDIName == null || pJNDIName.trim().length() == 0 ) {
         mJNDIName = "ejb/jmx/ejb/Adaptor";
      } else {
         mJNDIName = pJNDIName;
      }
      if( pJNDIServer == null || pJNDIServer.trim().length() == 0 ) {
         mJNDIServer = null;
      } else {
         mJNDIServer = pJNDIServer;
      }
      start( null );
   }
   
   // -------------------------------------------------------------------------
   // Methods
   // -------------------------------------------------------------------------  
   
   protected Adaptor getAdaptorBean( String pJNDIName )
      throws NamingException,
             RemoteException,
             CreateException
   {
      Context ctx = null;
      // The Adaptor can be registered on another JNDI-Server therefore
      // the user can overwrite the Provider URL
      if( mJNDIServer != null ) {
         Hashtable lProperties = new Hashtable();
         lProperties.put( Context.PROVIDER_URL, mJNDIServer );
         ctx = new InitialContext( lProperties );
      }
      else {
         ctx = new InitialContext();
      }
      
      Object aEJBRef = ctx.lookup( pJNDIName );
      AdaptorHome aHome = (AdaptorHome) 
         PortableRemoteObject.narrow( aEJBRef, AdaptorHome.class );

      ctx.close();
      
      return aHome.create();
   }

   /**
    * jmx:managed-operation
    */
   public void start(Object pServer) throws Exception
   {
      mRemoteAdaptor = getAdaptorBean( mJNDIName );
   }
   
   /**
    * jmx:managed-operation
    */ 
  public String getServerDescription() {
      return String.valueOf(mJNDIServer);
   }
}
