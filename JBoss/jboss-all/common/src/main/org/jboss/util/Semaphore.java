/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.util;

import java.util.Iterator;
import java.util.Map;
import java.util.HashMap;
import java.util.LinkedList;
import java.io.StringWriter;
import java.io.PrintWriter;

/**
 * Semaphore that can allow a specified number of threads to enter, blocking the
 * others. If the specified number of threads is 1, it acts as an exclusive semaphore
 * and can be used instead of synchronized blocks
 *
 * @author <a href="mailto:simone.bordet@compaq.com">Simone Bordet</a>
 * @version $Revision: 1.1 $
 */
public class Semaphore 
   implements Sync
{
   // Constants -----------------------------------------------------
   private static final long DEADLOCK_TIMEOUT = 5*60*1000;

   // Attributes ----------------------------------------------------
   private final static boolean m_debug = false;
   private int m_users;
   private int m_allowed;
   private Map m_logMap;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   public Semaphore(int allowed)
   {
      if (allowed < 1) throw new IllegalArgumentException();
		
      m_users = 0;
      m_allowed = allowed;
      m_logMap = new HashMap();
   }

   // Public --------------------------------------------------------
   public int getUsers() 
   {
      synchronized (this)
      {
         return m_users;
      }
   }

   // Sync implementation ----------------------------------------------
   public void acquire() throws InterruptedException
   {
      synchronized (this)
      {
         logAcquire();
			
         // One user more called acquire, increase users
         ++m_users;
         boolean waitSuccessful = false;
         while (m_allowed <= 0)
         {
            waitSuccessful = waitImpl(this);
            if (!waitSuccessful) 
            {
               // Dealock was detected, restore status, 'cause it's like a release()
               // that will probably be never called
               --m_users;
               ++m_allowed;
            }
         }
         --m_allowed;
      }
   }

   public void release()
   {
      synchronized (this)
      {
         logRelease();
			
         --m_users;
         ++m_allowed;
         notify();
      }
   }

   // Object overrides ---------------------------------------------------
   public String toString()
   {
      return super.toString() + " - " + m_users;
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------
   protected boolean waitImpl(Object lock) throws InterruptedException
   {
      // Wait (forever) until notified. To discover deadlocks,
      // turn on debugging of this class
      long start = System.currentTimeMillis();
      lock.wait(DEADLOCK_TIMEOUT);
      long end = System.currentTimeMillis();

      if ((end - start) > (DEADLOCK_TIMEOUT - 1000))
      {
         logDeadlock();
         return false;
      }
      return true;
   }
	
   protected void logAcquire()
   {
      if (m_debug) 
      {
         // Check if thread is already mapped
         Thread thread = Thread.currentThread();

         // Create stack trace
         StringWriter sw = new StringWriter();
         new Exception().printStackTrace(new PrintWriter(sw));
         String trace = sw.toString();
		
         LinkedList list = (LinkedList)m_logMap.get(thread);
         if (list != null)
         {
            // Thread is mapped
            // Add info
            Info prevInfo = (Info)list.getLast();
            Info info = new Info(thread, m_users, trace);
            list.add(info);
         }
         else
         {
            // Thread is not mapped, create list and add counter
            list = new LinkedList();
            Info info = new Info(thread, m_users, trace);
            list.add(info);
            // Map thread
            m_logMap.put(thread, list);
         }
      }
   }
   protected void logDeadlock()
   {
      System.err.println();
      System.err.println("DEADLOCK ON SEMAPHORE " + this);
      if (m_debug)
      {
         for (Iterator i = m_logMap.values().iterator(); i.hasNext();) 
         {
            LinkedList list = (LinkedList)i.next();
            for (Iterator iter = list.iterator(); iter.hasNext();)
            {
               System.err.println(iter.next());
            }
         }
      }
      System.err.println();
   }
   protected void logRelease()
   {
      if (m_debug)
      {
         // Find a matching thread and remove info for it
         Thread thread = Thread.currentThread();

         LinkedList list = (LinkedList)m_logMap.get(thread);
         if (list != null) 
         {
            Info info = new Info(thread, 0, "");
            if (!list.remove(info))
            {
               System.err.println("LOG INFO SIZE: " + list);
               new IllegalStateException("BUG: semaphore log list does not contain required info").printStackTrace();
            }

            // If no info left, remove the mapping
            int size = list.size();
            if (size < 1) 
            {
               m_logMap.remove(thread);
            }
         }			
         else 
         {
            throw new IllegalStateException("Semaphore log failed: release called without acquire");
         }
      }
   }

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
   private class Info
   {
      private Info(Thread t, int i, String s)
      {
         m_thread = t;
         m_counter = i;
         m_trace = s;
      }
      private Thread m_thread;
      private int m_counter;
      private String m_trace;
      public boolean equals(Object o) 
      {
         Info other = (Info)o;
         return m_thread == other.m_thread;
      }
      public String toString() 
      {
         return m_thread + " - " + m_counter + "\n" + m_trace;
      }
   }
}
