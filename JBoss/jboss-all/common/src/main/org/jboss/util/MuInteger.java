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
 * A mutable integer class.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class MuInteger
   extends MuNumber
{
   /** Integer value */
   private int value;
  
   /**
    * Construct a new mutable integer.
    */
   public MuInteger() {}

   /**
    * Construct a new mutable integer.
    *
    * @param i    Integer value.
    */
   public MuInteger(int i) {
      value = i;
   }

   /**
    * Construct a new mutable integer.
    *
    * @param obj  Object to convert to a <code>int</code> value.
    */
   public MuInteger(Object obj) {
      setValue(obj);
   }

   /**
    * Set the value to value only if the current value is equal to 
    * the assumed value.
    *
    * @param assumed  The assumed value.
    * @param i        The new value.
    * @return         True if value was changed.
    */
   public boolean commit(int assumed, int i) {
      boolean success = (assumed == value);
      if (success) value = i;
      return success;
   }

   /**
    * Swap values with another mutable integer.
    *
    * @param i    Mutable integer to swap values with.
    * @return     The new value.
    */
   public int swap(MuInteger i) {
      if (i == this) return i.value;

      int temp = value;
      value = i.value;
      i.value = temp;

      return value;
   }

   /**
    * Increment the value of this mutable integer.
    *
    * @return  Int value.
    */
   public int increment() {
      return ++value;
   }

   /**
    * Decrement the value of this mutable integer.
    *
    * @return  Int value.
    */
   public int decrement() {
      return --value;
   }

   /**
    * Add the specified amount.
    *
    * @param amount  Amount to add.
    * @return        The new value.
    */
   public int add(int amount) {
      return value += amount;
   }

   /**
    * Subtract the specified amount.
    *
    * @param amount  Amount to subtract.
    * @return        The new value.
    */
   public int subtract(int amount) {
      return value -= amount;
   }

   /**
    * Multiply by the specified factor.
    *
    * @param factor  Factor to multiply by.
    * @return        The new value.
    */
   public int multiply(int factor) {
      return value *= factor;
   }

   /**
    * Divide by the specified factor.
    *
    * @param factor  Factor to divide by.
    * @return        The new value.
    */
   public int divide(int factor) {
      return value /= factor;
   }

   /**
    * Set the value to the negative of its current value.
    *
    * @return     The new value.
    */
   public int negate() {
      value = ((int)-value);
      return value;
   }

   /**
    * Set the value to its complement.
    *
    * @return     The new value.
    */
   public int complement() {
      value = (int)~value;
      return value;
   }

   /**
    * <i>AND</i>s the current value with the specified value.
    *
    * @param b    Value to <i>and</i> with.
    * @return     The new value.
    */
   public int and(int b) {
      value = (int)(value & b);
      return value;
   }

   /**
    * <i>OR</i>s the current value with the specified value.
    *
    * @param b    Value to <i>or</i> with.
    * @return     The new value.
    */
   public int or(int b) {
      value = (int)(value | b);
      return value;
   }

   /**
    * <i>XOR</i>s the current value with the specified value.
    *
    * @param b    Value to <i>xor</i> with.
    * @return     The new value.
    */
   public int xor(int b) {
      value = (int)(value ^ b);
      return value;
   }

   /**
    * Shift the current value to the <i>right</i>.
    *
    * @param bits    The number of bits to shift.
    * @return        The new value.
    */
   public int shiftRight(int bits) {
      value >>= bits;
      return value;
   }

   /**
    * Shift the current value to the <i>right</i> with a zero extension.
    *
    * @param bits    The number of bits to shift.
    * @return        The new value.
    */
   public int shiftRightZero(int bits) {
      value >>>= bits;
      return value;
   }

   /**
    * Shift the current value to the <i>left</i>.
    *
    * @param bits    The number of bits to shift.
    * @return        The new value.
    */
   public int shiftLeft(int bits) {
      value <<= bits;
      return value;
   }

   /**
    * Compares this object with the specified int for order.
    *
    * @param other   Value to compare with.
    * @return        A negative integer, zero, or a positive integer as
    *                this object is less than, equal to, or greater than
    *                the specified object.
    */
   public int compareTo(int other) {
      return (value < other) ? -1 : (value == other) ? 0 : 1;
   }

   /**
    * Compares this object with the specified object for order.
    *
    * @param other   Value to compare with.
    * @return        A negative integer, zero, or a positive integer as
    *                this object is less than, equal to, or greater than
    *                the specified object.
    *
    * @throws ClassCastException    Object is not a MuInteger.
    */
   public int compareTo(Object obj) {
      return compareTo((MuInteger)obj);
   }

   /**
    * Convert this mutable integer to a string.
    *
    * @return   String value
    */
   public String toString() {
      return String.valueOf(value);
   }

   /**
    * Get the hash code for this mutable integer.
    *
    * @return   Hash code.
    */
   public int hashCode() {
      return value;
   }

   /**
    * Test the equality of this mutable integer and another object.
    *
    * @param obj    Qbject to test equality with.
    * @return       True if object is equal.
    */
   public boolean equals(Object obj) {
      if (obj == this) return true;

      if (obj != null && obj.getClass() == getClass()) {
         return value == ((MuInteger)obj).intValue();
      }

      return false;
   }

   /**
    * Return a cloned copy of this mutable integer.
    *
    * @return   Cloned mutable integer.
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
    * Set the value of this mutable integer.
    *
    * @param value  Object to convert to an integer value.
    *
    * @throws NotCoercibleException    Can not convert to <code>int</code>.
    */
   public void setValue(Object obj) {
      if (obj instanceof Number) {
         value = ((Number)obj).intValue();
      }
      else {
         throw new NotCoercibleException("can not convert to 'int': " + obj);
      }
   }

   /**
    * Get the value of this mutable integer.
    *
    * @return   <code>java.lang.Integer</code> value.
    */
   public Object getValue() {
      return new Integer(value);
   }
}
