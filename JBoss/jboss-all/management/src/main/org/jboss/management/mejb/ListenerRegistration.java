/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.management.mejb;

import java.security.InvalidParameterException;
import java.rmi.RemoteException;
import java.util.ArrayList;
import java.util.List;

import javax.ejb.CreateException;
import javax.management.InstanceNotFoundException;
import javax.management.ListenerNotFoundException;
import javax.management.NotificationFilter;
import javax.management.NotificationListener;
import javax.management.ObjectName;
import javax.management.j2ee.ManagementHome;

import org.jboss.management.mejb.MEJB;

import org.jboss.logging.Logger;

/**
 * Root class of the JBoss JSR-77 implementation of
 * {@link javax.management.j2ee.ListenerRegistration ListenerRegistration}.
 *
 * @author  <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @version $Revision: 1.4.2.1 $
 */
public class ListenerRegistration
      implements javax.management.j2ee.ListenerRegistration
{
   // Constants -----------------------------------------------------

   public static final int NOTIFICATION_TYPE_RMI = 0;
   public static final int NOTIFICATION_TYPE_JMS = 1;
   public static final int NOTIFICATION_TYPE_POLLING = 2;

   // Attributes ----------------------------------------------------

   private ManagementHome mHome;
   private int mEventType = NOTIFICATION_TYPE_RMI;
   private String[] mOptions;
   private List mListeners = new ArrayList();

   // Static --------------------------------------------------------

   private static final Logger log = Logger.getLogger(ListenerRegistration.class);

   // Constructors --------------------------------------------------

   public ListenerRegistration(ManagementHome pHome, String[] pOptions)
   {
      if (pHome == null)
      {
         throw new InvalidParameterException("Home Interface must be specified");
      }
      mHome = pHome;
      mOptions = pOptions;
   }

   // Public --------------------------------------------------------

   // javax.management.j2ee.ListenerRegistration implementation -----

   public void addNotificationListener(
         ObjectName pName,
         NotificationListener pListener,
         NotificationFilter pFilter,
         Object pHandback
         )
         throws
         InstanceNotFoundException,
         RemoteException
   {
      MEJB lManagement = null;
      // Create the remote MBean and register it
      try
      {
         // Get EJB
         lManagement = getMEJB();
         ClientNotificationListener lListener = null;
         switch (mEventType)
         {
            case NOTIFICATION_TYPE_RMI:
               lListener = new RMIClientNotificationListener(
                     pName,
                     pListener,
                     pHandback,
                     pFilter,
                     lManagement
               );
               break;
            case NOTIFICATION_TYPE_JMS:
               lListener = new JMSClientNotificationListener(
                     pName,
                     pListener,
                     pHandback,
                     pFilter,
                     mOptions[0],
                     mOptions[1], // JNDI-Server name
                     lManagement
               );
               break;
            case NOTIFICATION_TYPE_POLLING:
               lListener = new PollingClientNotificationListener(
                     pName,
                     pListener,
                     pHandback,
                     pFilter,
                     5000, // Sleeping Period
                     2500, // Maximum Pooled List Size
                     lManagement
               );
         }
         // Add this listener on the client to remove it when the client goes down
         mListeners.add(lListener);
      }
      catch (Exception e)
      {
         if (e instanceof RuntimeException)
         {
            throw (RuntimeException) e;
         }
         if (e instanceof InstanceNotFoundException)
         {
            throw (InstanceNotFoundException) e;
         }
         throw new RuntimeException("Remote access to perform this operation failed: " + e.getMessage());
      }
      finally
      {
         if (lManagement != null)
         {
            try
            {
               lManagement.remove();
            }
            catch (Exception e)
            {
               log.error("operation failed", e);
            }
         }
      }
   }

   public void removeNotificationListener(
         ObjectName pName,
         NotificationListener pListener
         )
         throws
         InstanceNotFoundException,
         ListenerNotFoundException,
         RemoteException
   {
      MEJB lManagement = null;
      try
      {
         // Get EJB
         lManagement = getMEJB();

         ClientNotificationListener lCheck = new SearchClientNotificationListener(pName, pListener);
         int i = mListeners.indexOf(lCheck);
         if (i >= 0)
         {
            ClientNotificationListener lListener = (ClientNotificationListener) mListeners.get(i);
            lListener.removeNotificationListener(lManagement);
         }
      }
      catch (Exception e)
      {
         if (e instanceof RuntimeException)
         {
            throw (RuntimeException) e;
         }
         if (e instanceof InstanceNotFoundException)
         {
            throw (InstanceNotFoundException) e;
         }
         throw new RuntimeException("Remote access to perform this operation failed: " + e.getMessage());
      }
      finally
      {
         if (lManagement != null)
         {
            try
            {
               lManagement.remove();
            }
            catch (Exception e)
            {
               log.error("operation failed", e);
            }
         }
      }
   }

   // Y overrides ---------------------------------------------------

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   private MEJB getMEJB()
         throws
         CreateException,
         RemoteException
   {
      Object lTemp = mHome.create();
      MEJB lReturn = (MEJB) lTemp;
      return lReturn;
   }

   // Inner classes -------------------------------------------------
}
