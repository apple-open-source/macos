/*
 * LGPL
 */
package javax.management;

public interface NotificationBroadcaster {

   public void addNotificationListener(NotificationListener listener,
                                       NotificationFilter filter,
                                       java.lang.Object handback)
   throws java.lang.IllegalArgumentException;

   public void removeNotificationListener(NotificationListener listener)
   throws ListenerNotFoundException;

   public MBeanNotificationInfo[] getNotificationInfo();



}
