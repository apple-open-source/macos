/*
 * LGPL
 */
package javax.management;

public interface NotificationListener extends java.util.EventListener {

   public void handleNotification(Notification notification,
                                  java.lang.Object handback);



}

