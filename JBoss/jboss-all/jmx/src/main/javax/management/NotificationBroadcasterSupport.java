/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

import org.jboss.logging.Logger;

/**
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.4.4.2 $
 *
 */
public class NotificationBroadcasterSupport implements NotificationBroadcaster
{

   public NotificationBroadcasterSupport()
   {
     // can not call this(Class) because we need to call getClass()
     this.log = Logger.getLogger(getClass().getName());     
   }

   public void addNotificationListener(NotificationListener listener, NotificationFilter filter, Object handback)
   {
      synchronized (listenerMap)
      {
         Map hbMap = (Map)listenerMap.get(listener);

         if (hbMap != null)
         {
            hbMap.put(handback, filter);
         }
         else
         {
            hbMap = new HashMap();
            hbMap.put(handback, filter);
            listenerMap.put(listener, hbMap);
         }
      }
   }

   public void removeNotificationListener(NotificationListener listener) throws ListenerNotFoundException
   {
      synchronized (listenerMap)
      {
         listenerMap.remove(listener);
      }
   }

   public MBeanNotificationInfo[] getNotificationInfo()
   {
      return null;
   }

   public void sendNotification(Notification notification)
   {
      boolean debug = log.isDebugEnabled();

      ArrayList copy = null;
      synchronized (listenerMap)
      {
         copy = new ArrayList(listenerMap.keySet());
      }
      for (int i = 0; i < copy.size(); i++)
      {
         NotificationListener listener = (NotificationListener)copy.get(i);
         Map hbMap = (Map)listenerMap.get(listener);
         
         // hbMap may be null if the listener was removed from the listenerMap during this loop
         if (hbMap == null) continue;
         
         Iterator it = hbMap.keySet().iterator();

         while(it.hasNext())
         {
            Object hb = it.next();
            NotificationFilter filter = (NotificationFilter)hbMap.get(hb);

            // if one listener throws an exception, it will not prevent
            // the broadcaster from notifying the other listeners
            try
            {
              if (filter == null)
                 listener.handleNotification(notification, hb);
              else if (filter.isNotificationEnabled(notification))
                 listener.handleNotification(notification, hb);
            }
            catch (Throwable th)
            {
              if (debug) log.debug("sendNotification() failed for listener: " + listener, th);
            }
         }
      }
   }
   
   
  protected Map getListenerMap()
  {
    return listenerMap;
  }


  // Attributes ------------------------------------------------------------

  /**
   * The map of notification listeners. Each listener can register with a map of filter/hb pairs
   * 
   */
  private Map listenerMap = new HashMap();


  /**
   * The instance logger for the service.  Not using a class logger
   * because we want to dynamically obtain the logger name from
   * concreate sub-classes.
   */
  protected Logger log;
  
  

}

