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
 * A mutable short class.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class MuShort
   extends MuNumber
{
   /** Short value */
   private short value;

   /**
    * Construct a new mutable short integer.
    */
   public MuShort() {}

   /**
    * Construct a new mutable short integer.
    *
    * @param s    Short value.
    */
   public MuShort(short s) {
      value = s;
   }

   /**
    * Construct a new mutable short integer.
    *
    * @param obj  Object to convert to a <code>short</code> value.
    */
   public MuShort(Object obj) {
      setValue(obj);
   }

   /**
    * Set the value.
    *
    * @param s    <code>short</code> value.
    * @return     The previous value.
    */
   public short set(short s) {
      short old = value;
      value = s;
      return old;
   }

   /**
    * Get the current value.
    *
    * @return     The current value.
    */
   public short get() {
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
   public boolean commit(short assumed, short b) {
      boolean success = (assumed == value);
      if (success) value = b;
      return success;
   }

   /**
    * Swap values with another mutable short.
    *
    * @param b       Mutable short to swap values with.
    * @return        The new value.
    */
   public short swap(MuShort b) {
      if (b == this) return value;

      short temp = value;
      value = b.value;
      b.value = temp;

      return value;
   }

   /**
    * Increment the value of this mutable short.
    *
    * @return  Short value.
    */
   public short increment() {
      return ++value;
   }

   /**
    * Decrement the value of this mutable short.
    *
    * @return  Short value.
    */
   public short decrement() {
      return --value;
   }

   /**
    * Add the specified amount.
    *
    * @param amount  Amount to add.
    * @return        The new value.
    */
   public short add(short amount) {
      return value += amount;
   }

   /**
    * Subtract the specified amount.
    *
    * @param amount  Amount to subtract.
    * @return        The new value.
    */
   public short subtract(short amount) {
      return value -= amount;
   }

   /**
    * Multiply by the specified factor.
    *
    * @param factor  Factor to multiply by.
    * @return        The new value.
    */
   public short multiply(short factor) {
      return value *= factor;
   }

   /**
    * Divide by the specified factor.
    *
    * @param factor  Factor to divide by.
    * @return        The new value.
    */
   public short divide(short factor) {
      return value /= factor;
   }

   /**
    * Set the value to the negative of its current value.
    *
    * @return     The new value.
    */
   public short negate() {
      value = ((short)-value);
      return value;
   }

   /**
    * Set the value to its complement.
    *
    * @return     The new value.
    */
   public short complement() {
      value = (short)~value;
      return value;
   }

   /**
    * <i>AND</i>s the current value with the specified value.
    *
    * @param b    Value to <i>and</i> with.
    * @return     The new value.
    */
   public short and(short b) {
      value = (short)(value & b);
      return value;
   }

   /**
    * <i>OR</i>s the current value with the specified value.
    *
    * @param b    Value to <i>or</i> with.
    * @return     The new value.
    */
   public short or(short b) {
      value = (short)(value | b);
      return value;
   }

   /**
    * <i>XOR</i>s the current value with the specified value.
    *
    * @param b    Value to <i>xor</i> with.
    * @return     The new value.
    */
   public short xor(short b) {
      value = (short)(value ^ b);
      return value;
   }

   /**
    * Shift the current value to the <i>right</i>.
    *
    * @param bits    The number of bits to shift.
    * @return        The new value.
    */
   public short shiftRight(int bits) {
      value >>= bits;
      return value;
   }

   /**
    * Shift the current value to the <i>right</i> with a zero extension.
    *
    * @param bits    The number of bits to shift.
    * @return        The new value.
    */
   public short shiftRightZero(int bits) {
      value >>>= bits;
      return value;
   }

   /**
    * Shift the current value to the <i>left</i>.
    *
    * @param bits    The number of bits to shift.
    * @return        The new value.
    */
   public short shiftLeft(int bits) {
      value <<= bits;
      return value;
   }

   /**
    * Compares this object with the specified short for order.
    *
    * @param other   Value to compare with.
    * @return        A negative integer, zero, or a positive integer as
    *                this object is less than, equal to, or greater than
    *                the specified object.
    */
   public int compareTo(short other) {
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
    * @throws ClassCastException    Object is not a MuShort.
    */
   public int compareTo(Object obj) {
      return compareTo((MuShort)obj);
   }

   /**
    * Convert this mutable short integer to a string.
    *
    * @return   String value.
    */
   public String toString() {
      return String.valueOf(value);
   }

   /**
    * Get the hash code for this mutable short integer.
    *
    * @return   Hash code.
    */
   public int hashCode() {
      return (int)value;
   }

   /**
    * Test the equality of this mutable short integer and another object.
    *
    * @param obj    Object to test equality with.
    * @return       True if object is equal.
    */
   public boolean equals(Object obj) {
      if (obj == this) return true;

      if (obj != null && obj.getClass() == getClass()) {
         return value == ((MuShort)obj).shortValue();
      }

      return false;
   }

   /**
    * Return a cloned copy of this mutable short.
    *
    * @return   Cloaned mutable short.
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
    * Set the value of this mutable short integer.
    *
    * @param obj  Object to convert to a <code>short</code> value.
    *
    * @throws NotCoercibleException    Can not convert to <code>short</code>.
    */
   public void setValue(Object obj) {
      if (obj instanceof Number) {
         value = ((Number)obj).shortValue();
      }
      else {
         throw new NotCoercibleException("can not convert to 'short': " + obj);
      }
   }

   /**
    * Return the value of this mutable short integer.
    *
    * @return   <code>java.lang.Short</code> value.
    */
   public Object getValue() {
      return new Short(value);
   }
}
