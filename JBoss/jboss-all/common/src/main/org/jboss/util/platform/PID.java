/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.platform;

import java.io.Serializable;

import java.util.Random;

/**
 * Provides access to the process identifier for this virtual machine.
 *
 * <p>Currently does not support native access and generates random numbers
 *    for the process id.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class PID
   implements Serializable, Cloneable
{
   /** The <tt>int</tt> process identifier. */
   protected final int id;

   /**
    * Construct a new PID.
    *
    * @param id   Process identifier.
    */
   protected PID(final int id) {
      this.id = id;
   }

   /**
    * Get the <tt>int</tt> process identifier.
    *
    * @return  <tt>int</tt> process identifier.
    */
   public final int getID() {
      return id;
   }

   /**
    * Return a string representation of this PID.
    *
    * @return  A string representation of this PID.
    */
   public String toString() {
      return String.valueOf(id);
   }

   /**
    * Return a string representation of this PID.
    *
    * @return  A string representation of this PID.
    */
   public String toString(int radix) {
      return Integer.toString(id, radix);
   }

   /**
    * Return the hash code of this PID.
    *
    * @return  The hash code of this PID.
    */
   public int hashCode() {
      return id;
   }

   /**
    * Check if the given object is equal to this PID.
    *
    * @param obj     Object to test equality with.
    * @return        True if object is equals to this PID.
    */
   public boolean equals(final Object obj) {
      if (obj == this) return true;

      if (obj != null && obj.getClass() == getClass()) {
         PID pid = (PID)obj;
         return pid.id == id;
      }

      return false;
   }

   /**
    * Returns a copy of this PID.
    *
    * @return  A copy of this PID.
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
   //                            Instance Access                          //
   /////////////////////////////////////////////////////////////////////////

   /** The single instance of PID for the running Virtual Machine */
   private static PID instance = null;

   /**
    * Get the PID for the current virtual machine.
    *
    * @return  Process identifier.
    */
   public synchronized static PID getInstance() {
      if (instance == null) {
         instance = create();
      }
      return instance;
   }

   /**
    * Create the PID for the current virtual mahcine.
    *
    * @return  Process identifier.
    */
   private static PID create() {
      // for now just return a random integer.
      int random = Math.abs(new Random().nextInt());
      return new PID(random);
   }
}
