/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.management.mejb;

import javax.management.NotificationListener;
import javax.management.ObjectName;

/**
 * Local Listener only to seach other Local Listeners
 **/
public class SearchClientNotificationListener
      extends ClientNotificationListener
{

   public SearchClientNotificationListener(ObjectName pSender,
         NotificationListener pClientListener)
   {
      super(pSender, pClientListener, null);
   }

}
