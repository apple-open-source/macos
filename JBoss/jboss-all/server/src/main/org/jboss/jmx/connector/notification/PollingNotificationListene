/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.jmx.connector.notification;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.List;

import javax.management.Notification;
import javax.management.NotificationListener;

/** 
 * Remote Listener using Polling to send the event
 *
 * @jmx:mbean extends="org.jboss.jmx.connector.notification.ListenerMBean"
 * 
 * @version <tt>$Revision: 1.2 $</tt>
 * @author <A href="mailto:andreas@jboss.org">Andreas &quot;Mad&quot; Schaefer</A>
 **/
public class PollingNotificationListener
   implements PollingNotificationListenerMBean
{
   private List mList;
   private int mMaximumSize = 1000;
   
   public PollingNotificationListener(
      int pListSize,
      int pMaximumListSize
   ) {
      if( pListSize <= 0 ) {
         pListSize = 1000;
      }
      mList = new ArrayList( pListSize );
      if( pMaximumListSize > 0 && pMaximumListSize > pListSize ) {
         mMaximumSize = pMaximumListSize;
      }
   }

   /**
   * Handles the given notification by sending this to the remote
   * client listener
   *
   * @param pNotification    Notification to be send
   * @param pHandback        Handback object
   */
   public void handleNotification(
      Notification pNotification,
      Object pHandback
   ) {
      synchronized( mList ) {
         if( mList.size() <= mMaximumSize ) { 
            mList.add( pNotification );
         }
      }
   }

   /**
    * jmx:managed-operation
    */
   public List getNotifications() {
      return getNotifications( mMaximumSize );
   }

   /**
    * jmx:managed-operation
    */
   public List getNotifications( int pMaxiumSize ) {
      List lReturn = null;
      synchronized( mList ) {
         pMaxiumSize = pMaxiumSize > mList.size() ? mList.size() : pMaxiumSize;
         lReturn = new ArrayList( mList.subList( 0, pMaxiumSize ) );
         mList.removeAll( lReturn );
      }
      
      return lReturn;
   }
}
