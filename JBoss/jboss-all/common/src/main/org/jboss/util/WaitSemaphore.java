/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.util;

/**
 * Wait exclusive semaphore with wait - notify primitives
 *
 * @author <a href="mailto:simone.bordet@compaq.com">Simone Bordet</a>
 * @version $Revision: 1.1 $
 */
public class WaitSemaphore
   extends Semaphore
   implements WaitSync
{
   // Constants -----------------------------------------------------
   private final static int MAX_USERS_ALLOWED = 1;

   // Attributes ----------------------------------------------------
   private int m_waiters;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   public WaitSemaphore()
   {
      super(MAX_USERS_ALLOWED);
   }

   // Public --------------------------------------------------------
   public void doWait() throws InterruptedException
   {
      synchronized (this)
      {
         release();
         ++m_waiters;
         waitImpl(this);
         --m_waiters;
         acquire();
      }
   }

   public void doNotify() throws InterruptedException
   {
      synchronized (this)
      {
         if (getWaiters() > 0)
         {
            acquire();
            notify();
            release();
         }
      }
   }

   public int getWaiters()
   {
      synchronized (this)
      {
         return m_waiters;
      }
   }

   // Object overrides ---------------------------------------------------
   public String toString()
   {
      return super.toString() + " - " + m_waiters;
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
}
