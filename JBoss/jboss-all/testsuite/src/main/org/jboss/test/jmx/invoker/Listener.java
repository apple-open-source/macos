package org.jboss.test.jmx.invoker;

import java.rmi.RemoteException;
import java.rmi.server.UnicastRemoteObject;
import javax.management.Notification;
import org.jboss.jmx.adaptor.rmi.RMINotificationListener;

/** An RMI callback implementation used to receive remote JMX notifications
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class Listener implements RMINotificationListener
{
   int count;

   public int getCount()
   {
      return count;
   }
   public void export() throws RemoteException
   {
      UnicastRemoteObject.exportObject(this);
   }
   public void unexport() throws RemoteException
   {
      UnicastRemoteObject.unexportObject(this, true);
   }
   public void handleNotification(Notification event, Object handback)
   {
      System.out.println("handleNotification, event: "+event+", count="+count);
      count ++;
   }
}
