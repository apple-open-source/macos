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
 * A mutable long integer class.
 *
 * @version <tt>$Revision: 1.2 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class MuLong
   extends MuNumber
{
   /** Long value */
   private long value;

   /**
    * Construct a new mutable long integer.
    */
   public MuLong() {}

   /**
    * Construct a new mutable long integer.
    *
    * @param l    <code>long</code> value.
    */
   public MuLong(long l) {
      value = l;
   }

   /**
    * Construct a new mutable long integer.
    *
    * @param obj  Object to convert to a <code>long</code> value.
    */
   public MuLong(Object obj) {
      setValue(obj);
   }

   /**
    * Set the value to value only if the current value is equal to 
    * the assumed value.
    *
    * @param assumed  The assumed value.
    * @param b        The new value.
    * @return         True if value was changed.
    */
   public boolean commit(long assumed, long b) {
      boolean success = (assumed == value);
      if (success) value = b;
      return success;
   }

   /**
    * Swap values with another mutable long.
    *
    * @param b       Mutable long to swap values with.
    * @return        The new value.
    */
   public long swap(MuLong b) {
      if (b == this) return value;

      long temp = value;
      value = b.value;
      b.value = temp;

      return value;
   }

   /**
    * Increment the value of this mutable long.
    *
    * @return  Long value.
    */
   public long increment() {
      return ++value;
   }

   /**
    * Decrement the value of this mutable long.
    *
    * @return  Long value.
    */
   public long decrement() {
      return --value;
   }

   /**
    * Add the specified amount.
    *
    * @param amount  Amount to add.
    * @return        The new value.
    */
   public long add(long amount) {
      return value += amount;
   }

   /**
    * Subtract the specified amount.
    *
    * @param amount  Amount to subtract.
    * @return        The new value.
    */
   public long subtract(long amount) {
      return value -= amount;
   }

   /**
    * Multiply by the specified factor.
    *
    * @param factor  Factor to multiply by.
    * @return        The new value.
    */
   public long multiply(long factor) {
      return value *= factor;
   }

   /**
    * Divide by the specified factor.
    *
    * @param factor  Factor to divide by.
    * @return        The new value.
    */
   public long divide(long factor) {
      return value /= factor;
   }

   /**
    * Set the value to the negative of its current value.
    *
    * @return     The new value.
    */
   public long negate() {
      value = ((long)-value);
      return value;
   }

   /**
    * Set the value to its complement.
    *
    * @return     The new value.
    */
   public long complement() {
      value = (long)~value;
      return value;
   }

   /**
    * <i>AND</i>s the current value with the specified value.
    *
    * @param b    Value to <i>and</i> with.
    * @return     The new value.
    */
   public long and(long b) {
      value = (long)(value & b);
      return value;
   }

   /**
    * <i>OR</i>s the current value with the specified value.
    *
    * @param b    Value to <i>or</i> with.
    * @return     The new value.
    */
   public long or(long b) {
      value = (long)(value | b);
      return value;
   }

   /**
    * <i>XOR</i>s the current value with the specified value.
    *
    * @param b    Value to <i>xor</i> with.
    * @return     The new value.
    */
   public long xor(long b) {
      value = (long)(value ^ b);
      return value;
   }

   /**
    * Shift the current value to the <i>right</i>.
    *
    * @param bits    The number of bits to shift.
    * @return        The new value.
    */
   public long shiftRight(int bits) {
      value >>= bits;
      return value;
   }

   /**
    * Shift the current value to the <i>right</i> with a zero extension.
    *
    * @param bits    The number of bits to shift.
    * @return        The new value.
    */
   public long shiftRightZero(int bits) {
      value >>>= bits;
      return value;
   }

   /**
    * Shift the current value to the <i>left</i>.
    *
    * @param bits    The number of bits to shift.
    * @return        The new value.
    */
   public long shiftLeft(int bits) {
      value <<= bits;
      return value;
   }

   /**
    * Compares this object with the specified long for order.
    *
    * @param other   Value to compare with.
    * @return        A negative integer, zero, or a positive integer as
    *                this object is less than, equal to, or greater than
    *                the specified object.
    */
   public int compareTo(long other) {
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
    * @throws ClassCastException    Object is not a MuLong.
    */
   public int compareTo(Object obj) {
      return compareTo((MuLong)obj);
   }

   /**
    * Convert this mutable long integer to a string.
    *
    * @return   String value.
    */
   public String toString() {
      return String.valueOf(value);
   }

   /**
    * Get the hash code of this mutable long integer.
    *
    * @return   Hash code.
    */
   public int hashCode() {
      return HashCode.generate(value);
   }

   /**
    * Test the equality of this mutable long integer and another object.
    *
    * @param obj    Object to test equality with.
    * @return       True if object is equal.
    */
   public boolean equals(Object obj) {
      if (obj == this) return true;

      if (obj != null && obj.getClass() == getClass()) {
         return value == ((MuLong)obj).longValue();
      }

      return false;
   }

   /**
    * Return a cloned copy of this mutable long.
    *
    * @return   Cloaned mutable long.
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

   /**
    * Set the value of this mutable long integer.
    *
    * @param value    The new value.
    */
   public void set(long value)
   {
      this.value = value;
   }


   /////////////////////////////////////////////////////////////////////////
   //                            Mutable Support                          //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Set the value of this mutable long integer.
    *
    * @param obj  Object to convert to a <code>long</code> value.
    *
    * @throws NotCoercibleException    Can not convert to <code>long</code>.
    */
   public void setValue(Object obj) {
      if (obj instanceof Number) {
         value = ((Number)obj).longValue();
      }
      else {
         throw new NotCoercibleException("can not convert to 'long': " + obj);
      }
   }

   /**
    * Return the value of this mutable long integer.
    *
    * @return   <code>java.lang.Long<code> value.
    */
   public Object getValue() {
      return new Long(value);
   }
}
