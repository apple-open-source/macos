/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.jmx.connector.notification;

import java.io.Serializable;

import javax.jms.JMSException;
import javax.jms.Message;
import javax.jms.MessageListener;
import javax.jms.ObjectMessage;

import javax.management.Notification;
import javax.management.NotificationListener;
import javax.management.ObjectName;

import org.jboss.jmx.connector.RemoteMBeanServer;

/**
* Local Listener only to seach other Local Listeners
**/
public class SearchClientNotificationListener
   extends ClientNotificationListener
{

   public SearchClientNotificationListener(
      ObjectName pSender,
      NotificationListener pClientListener
   ) {
      super( pSender, pClientListener, null );
   }

}
