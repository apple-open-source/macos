package org.jboss.monitor;

public class LockMonitor
{
   long total_time = 0;
   long num_contentions = 0;
   public long timeouts = 0;
   long maxContenders = 0;
   long currentContenders = 0;
   private EntityLockMonitor monitor;

   public LockMonitor(EntityLockMonitor monitor)
   {
      this.monitor = monitor;
   }

   public void contending()
   {
   	  synchronized(this)
   	  {
   	     num_contentions++;
         currentContenders++;
         
         if (currentContenders > maxContenders)
            maxContenders = currentContenders;
	  }
      
      // Remark Ulf Schroeter: DO NOT include following call into the
      // synchronization block because it will cause deadlocks between
      // LockMonitor methods and EntityLockMonitor.clearMonitor() call ! 
      monitor.incrementContenders();
   }

   public void finishedContending(long time)
   {
      synchronized(this)
      {	
         total_time += time;
         currentContenders--;
	  }

	// Remark Ulf Schroeter: DO NOT include following call into the
	// synchronization block because it will cause deadlocks between
	// LockMonitor methods and EntityLockMonitor.clearMonitor() call ! 
      monitor.decrementContenders(time);
   }
}

