package org.jboss.util.deadlock;

import java.util.HashMap;
import java.util.HashSet;

/**
 * Created by IntelliJ IDEA.
 * User: wburke
 * Date: Aug 21, 2003
 * Time: 2:10:46 PM
 * To change this template use Options | File Templates.
 */
public class DeadlockDetector
{
   // TODO Maybe this should be an MBean in the future
   public static DeadlockDetector singleton = new DeadlockDetector();
   // This following is for deadlock detection
   protected HashMap waiting = new HashMap();

   public void deadlockDetection(Object holder, Resource resource)
           throws ApplicationDeadlockException
   {
      HashSet set = new HashSet();
      set.add(holder);

      Object checkHolder = resource.getResourceHolder();

      synchronized (waiting)
      {
         addWaiting(holder, resource);

         while (checkHolder != null)
         {
            Resource waitingFor = (Resource)waiting.get(checkHolder);
            Object holding = null;
            if (waitingFor != null)
            {
               holding = waitingFor.getResourceHolder();
            }
            if (holding != null)
            {
               if (set.contains(holding))
               {
                  // removeWaiting should be cleaned up in acquire
                  String msg = "Application deadlock detected, resource="+resource
                     +", holder="+holder+", waitingResource="+waitingFor
                     +", waitingResourceHolder="+holding;
                  throw new ApplicationDeadlockException(msg, true);
               }
               set.add(holding);
            }
            checkHolder = holding;
         }
      }
   }

   /**
    * Add a transaction waiting for a lock
    */
   public void addWaiting(Object holder, Resource resource)
   {
      synchronized (waiting)
      {
         waiting.put(holder, resource);
      }
   }

   /**
    * Remove a transaction waiting for a lock
    */
   public void removeWaiting(Object holder)
   {
      synchronized (waiting)
      {
         waiting.remove(holder);
      }
   }

}
