/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.jmx.connector.notification;

import java.io.Serializable;
import java.util.Random;

import javax.management.InstanceNotFoundException;
import javax.management.InstanceAlreadyExistsException;
import javax.management.JMException;
import javax.management.MalformedObjectNameException;
import javax.management.MBeanRegistrationException;
import javax.management.MBeanException;
import javax.management.NotCompliantMBeanException;
import javax.management.Notification;
import javax.management.NotificationFilter;
import javax.management.NotificationListener;
import javax.management.ObjectInstance;
import javax.management.ObjectName;
import javax.management.ReflectionException;

import org.jboss.jmx.connector.RemoteMBeanServer;

import org.jboss.logging.Logger;

/**
 * Basic Local Listener to receive Notification from a remote JMX Agent
 *
 * @version <tt>$Revision: 1.3 $</tt>
 * @author <A href="mailto:andreas@jboss.org">Andreas &quot;Mad&quot; Schaefer</A>
 **/
public abstract class ClientNotificationListener
{
   protected Logger log = Logger.getLogger(this.getClass());

   private ObjectName               mSender;
   private ObjectName               mRemoteListener;
   protected NotificationListener   mClientListener;
   protected Object                 mHandback;
   private Random                   mRandom = new Random();
   
   public ClientNotificationListener(
      ObjectName pSender,
      NotificationListener pClientListener,
      Object pHandback
   ) {
      mSender = pSender;
      mClientListener = pClientListener;
      mHandback = pHandback;
   }
   
   public ObjectName createListener(
      RemoteMBeanServer pConnector,
      String mClass,
      Object[] pParameters,
      String[] pSignatures
   ) throws
      MalformedObjectNameException,
      ReflectionException,
      MBeanRegistrationException,
      MBeanException,
      NotCompliantMBeanException
   {
      ObjectName lName = null;
      while( lName == null ) {
         try {
            lName = new ObjectName( "JMX:type=listener,id=" + mRandom.nextLong() );
            ObjectInstance lInstance = pConnector.createMBean(
               mClass,
               lName,
               pParameters,
               pSignatures
            );
            lName = lInstance.getObjectName();
         }
         catch( InstanceAlreadyExistsException iaee ) {
            lName = null;
         }
      }
      mRemoteListener = lName;
      return lName;
   }

   public void addNotificationListener(
      RemoteMBeanServer pConnector,
      NotificationFilter pFilter
   ) throws
      InstanceNotFoundException
   {
      pConnector.addNotificationListener(
         mSender,
         mRemoteListener,
         pFilter,
         null
      );
   }
   
  public void removeNotificationListener(
      RemoteMBeanServer pConnector
   ) throws
      InstanceNotFoundException
   {
      try {
         pConnector.removeNotificationListener(
            mSender,
            mRemoteListener
         );
      }
      catch( JMException jme ) {
      }
      try {
         pConnector.unregisterMBean( mRemoteListener );
      }
      catch( JMException jme ) {
      }
   }
   
   public ObjectName getSenderMBean() {
      return mSender;
   }
   
   public ObjectName getRemoteListenerName() {
      return mRemoteListener;
   }

   public boolean equals( Object pTest ) {
      if( pTest instanceof ClientNotificationListener ) {
         ClientNotificationListener lListener = (ClientNotificationListener) pTest;
         return
            mSender.equals( lListener.mSender ) &&
            mClientListener.equals( lListener.mClientListener );
      }
      return false;
   }

}
