/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util;

/**
 * A mutable double class.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class MuDouble
   extends MuNumber
{
   /** Double value */
   private double value = 0;

   /**
    * Construct a new mutable double.
    */
   public MuDouble() {}

   /**
    * Construct a new mutable double.
    *
    * @param d    <code>double</code> value.
    */
   public MuDouble(double d) {
      value = d;
   }

   /**
    * Construct a new mutable double.
    *
    * @param obj  Object to convert to a <code>double</code> value.
    */
   public MuDouble(Object obj) {
      setValue(obj);
   }

   /**
    * Set the value.
    *
    * @param f    <code>double</code> value.
    * @return     The previous value.
    */
   public double set(double f) {
      double old = value;
      value = f;
      return old;
   }

   /**
    * Get the current value.
    *
    * @return  The current value.
    */
   public double get() {
      return value;
   }

   /**
    * Set the value to value only if the current value is equal to 
    * the assumed value.
    *
    * @param assumed  The assumed value.
    * @param b        The new value.
    * @return         True if value was changed.
    */
   public boolean commit(double assumed, double b) {
      boolean success = Primitives.equals(assumed, value);
      if (success) value = b;
      return success;
   }

   /**
    * Swap values with another mutable double.
    *
    * @param b       Mutable double to swap values with.
    * @return        The new value.
    */
   public double swap(MuDouble b) {
      if (b == this) return value;

      double temp = value;
      value = b.value;
      b.value = temp;

      return value;
   }

   /**
    * Add the specified amount.
    *
    * @param amount  Amount to add.
    * @return        The new value.
    */
   public double add(double amount) {
      return value += amount;
   }

   /**
    * Subtract the specified amount.
    *
    * @param amount  Amount to subtract.
    * @return        The new value.
    */
   public double subtract(double amount) {
      return value -= amount;
   }

   /**
    * Multiply by the specified factor.
    *
    * @param factor  Factor to multiply by.
    * @return        The new value.
    */
   public double multiply(double factor) {
      return value *= factor;
   }

   /**
    * Divide by the specified factor.
    *
    * @param factor  Factor to divide by.
    * @return        The new value.
    */
   public double divide(double factor) {
      return value /= factor;
   }

   /**
    * Set the value to the negative of its current value.
    *
    * @return     The new value.
    */
   public double negate() {
      value = ((double)-value);
      return value;
   }

   /**
    * Compares this object with the specified double for order.
    *
    * @param other   Value to compare with.
    * @return        A negative integer, zero, or a positive integer as
    *                this object is less than, equal to, or greater than
    *                the specified object.
    */
   public int compareTo(double other) {
      return (value < other) ? -1 : Primitives.equals(value, other) ? 0 : 1;
   }

   /**
    * Compares this object with the specified object for order.
    *
    * @param other   Value to compare with.
    * @return        A negative integer, zero, or a positive integer as
    *                this object is less than, equal to, or greater than
    *                the specified object.
    *
    * @throws ClassCastException    Object is not a MuDouble.
    */
   public int compareTo(Object obj) {
      return compareTo((MuDouble)obj);
   }

   /**
    * Convert this mutable double to a string.
    *
    * @return   String value.
    */
   public String toString() {
      return String.valueOf(value);
   }
   
   /**
    * Get the hash code for this mutable double.
    *
    * @return   Hash code.
    */
   public int hashCode() {
      return HashCode.generate(value);
   }

   /**
    * Test the equality of this mutable double with another object.
    *
    * @param obj    Object to test
    * @return       Is equal
    */
   public boolean equals(Object obj) {
      if (obj == this) return true;

      if (obj != null && obj.getClass() == getClass()) {
         return Primitives.equals(value, ((MuDouble)obj).doubleValue());
      }

      return false;
   }

   /**
    * Return a cloned copy of this mutable double.
    *
    * @return   Cloaned mutable double.
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
   //                             Number Support                          //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Return the <code>byte</code> value of this object.
    *
    * @return   <code>byte</code> value.
    */
   public byte byteValue() {
      return (byte)value;
   }

   /**
    * Return the <code>short</code> value of this object.
    *
    * @return   <code>short</code> value.
    */
   public short shortValue() {
      return (short)value;
   }

   /**
    * Return the <code>int</code> value of this object.
    *
    * @return   <code>int</code> value.
    */
   public int intValue() {
      return (int)value;
   }

   /**
    * Return the <code>long</code> value of this object.
    *
    * @return   <code>long</code> value.
    */
   public long longValue() {
      return (long)value;
   }

   /**
    * Return the <code>float</code> value of this object.
    *
    * @return   <code>float</code> value.
    */
   public float floatValue() {
      return (float)value;
   }

   /**
    * Return the <code>double</code> value of this object.
    *
    * @return   <code>double</code> value.
    */
   public double doubleValue() {
      return (double)value;
   }


   /////////////////////////////////////////////////////////////////////////
   //                            Mutable Support                          //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Set the value of this mutable double.
    *
    * @param obj  Object to convert to a <code>double</code> value.
    *
    * @throws NotCoercibleException    Can not convert to <code>double</code>.
    */
   public void setValue(Object obj) {
      if (obj instanceof Number) {
         value = ((Number)obj).doubleValue();
      }
      else {
         throw new NotCoercibleException("can not convert to 'double': " + obj);
      }
   }

   /**
    * Get the value of this mutable double.
    *
    * @return   <code>java.lang.Double</code> value
    */
   public Object getValue() {
      return new Double(value);
   }
}
