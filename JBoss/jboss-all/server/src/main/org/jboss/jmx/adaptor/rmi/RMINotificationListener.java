package org.jboss.jmx.adaptor.rmi;

import java.rmi.Remote;
import java.rmi.RemoteException;
import javax.management.Notification;

/** An RMIfied version of the javax.management.NotificationListener.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public interface RMINotificationListener extends Remote
{
   public void handleNotification(Notification notification, Object handback)
      throws RemoteException;
}
