/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.monitor.support;

import test.compliance.monitor.MonitorSUITE;

public class MonitorSupport 
{
   boolean done = false;
   String last = "set";
   public synchronized void lock(String who)
   {
      if (!done && last.equals(who))
      {
         try
         {
            wait(MonitorSUITE.MAX_WAIT);
         }
         catch (InterruptedException e) {}        
         if (!done && last.equals(who))
            throw new RuntimeException("-- Time Out --");
      }
   }
   public synchronized void unlock(String who)
   {
      if (!done && last.equals(who))
         throw new RuntimeException("-- Synchronization failure --");
      last=who; 
      notifyAll();
   }
   public synchronized void end()
   {
      done = true;
      notifyAll();
   }
}
