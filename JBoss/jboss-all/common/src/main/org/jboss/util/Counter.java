/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util;

import java.io.Serializable;

/**
 * An integer counter class.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class Counter
   implements Serializable, Cloneable
{
   /** The current count */
   private int count;

   /**
    * Construct a Counter with a starting value.
    *
    * @param count   Starting value for counter.
    */
   public Counter(final int count) {
      this.count = count;
   }

   /**
    * Construct a Counter.
    */
   public Counter() {}

   /**
    * Increment the counter. (Optional operation)
    *
    * @return  The incremented value of the counter.
    */
   public int increment() {
      return ++count;
   }

   /**
    * Decrement the counter. (Optional operation)
    *
    * @return  The decremented value of the counter.
    */
   public int decrement() {
      return --count;
   }

   /**
    * Return the current value of the counter.
    *
    * @return  The current value of the counter.
    */
   public int getCount() {
      return count;
   }

   /**
    * Reset the counter to zero. (Optional operation)
    */
   public void reset() {
      this.count = 0;
   }

   /**
    * Check if the given object is equal to this.
    *
    * @param obj  Object to test equality with.
    * @return     True if object is equal to this.
    */
   public boolean equals(final Object obj) {
      if (obj == this) return true;

      if (obj != null && obj.getClass() == getClass()) {
         return ((Counter)obj).count == count;
      }
      
      return false;
   }

   /**
    * Return a string representation of this.
    *
    * @return  A string representation of this.
    */
   public String toString() {
      return String.valueOf(count);
   }

   /**
    * Return a cloned copy of this object.
    *
    * @return  A cloned copy of this object.
    */
   public Object clone() {
      try {
         return super.clone();
      }
      catch (CloneNotSupportedException e) {
         throw new InternalError();
      }
   }


   /////////////////////////////////////////////////////////////////////////
   //                                Wrappers                             //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Base wrapper class for other wrappers.
    */
   private static class Wrapper
      extends Counter
   {
      /** The wrapped counter */
      protected final Counter counter;

      public Wrapper(final Counter counter) {
         this.counter = counter;
      }

      public int increment() {
         return counter.increment();
      }

      public int decrement() {
         return counter.decrement();
      }

      public int getCount() {
         return counter.getCount();
      }

      public void reset() {
         counter.reset();
      }

      public boolean equals(final Object obj) {
         return counter.equals(obj);
      }

      public String toString() {
         return counter.toString();
      }

      public Object clone() {
         return counter.clone();
      }
   }

   /**
    * Return a synchronized counter.
    *
    * @param counter    Counter to synchronize.
    * @return           Synchronized counter.
    */
   public static Counter makeSynchronized(final Counter counter) {
      return new Wrapper(counter) {
            public synchronized int increment() {
               return this.counter.increment();
            }

            public synchronized int decrement() {
               return this.counter.decrement();
            }

            public synchronized int getCount() {
               return this.counter.getCount();
            }

            public synchronized void reset() {
               this.counter.reset();
            }

            public synchronized int hashCode() {
               return this.counter.hashCode();
            }

            public synchronized boolean equals(final Object obj) {
               return this.counter.equals(obj);
            }

            public synchronized String toString() {
               return this.counter.toString();
            }

            public synchronized Object clone() {
               return this.counter.clone();
            }
         };
   }

   /**
    * Returns a directional counter.
    *
    * @param counter       Counter to make directional.
    * @param increasing    True to create an increasing only
    *                      or false to create a decreasing only.
    * @return              A directional counter.
    */
   public static Counter makeDirectional(final Counter counter,
                                         final boolean increasing)
   {
      Counter temp;
      if (increasing) {
         temp = new Wrapper(counter) {
               public int decrement() {
                  throw new UnsupportedOperationException();
               }

               public void reset() {
                  throw new UnsupportedOperationException();
               }
            };
      }
      else {
         temp = new Wrapper(counter) {
               public int increment() {
                  throw new UnsupportedOperationException();
               }
            };
      }
      
      return temp;
   }
}
