/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.util;

/**
 * A simple valve with no support for re-opening.
 *
 * @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public class Valve
{
   // Constants -----------------------------------------------------

   /** The valve is open */
   public static final int OPEN = 0;

   /** The valve is in holding state */
   public static final int CLOSING = 1;

   /** The valve is in hold state */
   public static final int CLOSED = 2;

   // Attributes ----------------------------------------------------

   /** The state lock */
   protected Object stateLock = new Object();

   /** The state */
   protected int state;

   /** The invocation count */
   protected int invocations = 0;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Create a valve in the open state
   */
   public Valve()
   {
      this(OPEN);
   }

   /**
    * Create a valve with the give initial state
    * 
    * @param state the initial state
    */
   protected Valve(int state)
   {
      this.state = state;
   }

   // Public --------------------------------------------------------

   /**
    * Are we closed?
    * 
    * @return true when closing or closed, false otherwise
    */
   public boolean isClosed()
   {
      synchronized (stateLock)
      {
         return state != OPEN;
      }
   }

   /**
    * Invoked before an invocation
    * 
    * @return true if allowed entry, false otherwise
    */
   public boolean beforeInvocation()
   {
      synchronized (stateLock)
      {
         if (state != OPEN)
            return false;
         ++invocations;
      }
      return true;
   }

   /**
    * Invoked after an invocation
    */
   public void afterInvocation()
   {
      synchronized (stateLock)
      {
         --invocations;
         stateLock.notifyAll();
      }
   }

   /**
    * Invoked before closing
    */
   public void closing()
   {
      boolean interrupted = false;
      synchronized (stateLock)
      {
         state = CLOSING;

         while (invocations > 0)
         {
            try
            {
               stateLock.wait();
            }
            catch (InterruptedException e)
            {
               interrupted = true;
            }
         }
      }
      if (interrupted)
         Thread.currentThread().interrupt();
   }

   /**
    * Invoked after closing
    */
   public void closed()
   {
      synchronized (stateLock)
      {
         state = CLOSED;
      }
   }

   // Protected -----------------------------------------------------

   // Package Private -----------------------------------------------

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------

}
