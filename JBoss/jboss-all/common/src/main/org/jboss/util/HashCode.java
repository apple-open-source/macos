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
 * A hash-code generator and a collection of static hash-code generation
 * methods.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public final class HashCode
   implements Serializable, Cloneable, Comparable
{
   /** Hashcode for a 'null' value. */
   private static final int NULL_HASHCODE = 0;

   /** Hashcode for a 'true' boolean */
   private static final int TRUE_HASHCODE = 1231; // completely arbitrary

   /** Hashcode for a 'false' boolean */
   private static final int FALSE_HASHCODE = 1237; // completely arbitrary

   /** The hash-code value. */
   private int value;

   /**
    * Construct a new <tt>HashCode</tt> using the given <tt>int</tt> as the
    * base value.
    *
    * @param value   <tt>int</tt> to use as the base value.
    */
   public HashCode(final int value) {
      this.value = value;
   }

   /**
    * Construct a new <tt>HashCode</tt>.
    */
   public HashCode() {
      this(0);
   }

   /**
    * Add the hash-code of the given value.
    *
    * @param b    Value to get hash-code from.
    * @return     <i>This</i> <tt>HashCode</tt>.
    */
   public HashCode add(final boolean b) {
      value ^= generate(b);
      return this;
   }

   /**
    * Add the hash-code of the given value.
    *
    * @param n    Value to get hash-code from.
    * @return     <i>This</i> <tt>HashCode</tt>.
    */
   public HashCode add(final byte n) {
      value ^= n;
      return this;
   }

   /**
    * Add the hash-code of the given value.
    *
    * @param n    Value to get hash-code from.
    * @return     <i>This</i> <tt>HashCode</tt>.
    */
   public HashCode add(final char n) {
      value ^= n;
      return this;
   }

   /**
    * Add the hash-code of the given value.
    *
    * @param n    Value to get hash-code from.
    * @return     <i>This</i> <tt>HashCode</tt>.
    */
   public HashCode add(final short n) {
      value ^= n;
      return this;
   }

   /**
    * Add the hash-code of the given value.
    *
    * @param n    Value to get hash-code from.
    * @return     <i>This</i> <tt>HashCode</tt>.
    */
   public HashCode add(final int n) {
      value ^= n;
      return this;
   }

   /**
    * Add the hash-code of the given value.
    *
    * @param n    Value to get hash-code from.
    * @return     <i>This</i> <tt>HashCode</tt>.
    */
   public HashCode add(final long n) {
      value ^= generate(n);
      return this;
   }

   /**
    * Add the hash-code of the given value.
    *
    * @param f    Value to get hash-code from.
    * @return     <i>This</i> <tt>HashCode</tt>.
    */
   public HashCode add(final float f) {
      value ^= generate(f);
      return this;
   }

   /**
    * Add the hash-code of the given value.
    *
    * @param f    Value to get hash-code from.
    * @return     <i>This</i> <tt>HashCode</tt>.
    */
   public HashCode add(final double f) {
      value ^= generate(f);
      return this;
   }

   /**
    * Add the hash-code of the given object.
    *
    * @param obj    Value to get hash-code from.
    * @return        <i>This</i> <tt>HashCode</tt>.
    */
   public HashCode add(final Object obj) {
      value ^= generate(obj);
      return this;
   }

   /**
    * Get the hash-code.
    *
    * @return   Hash-code.
    */
   public int hashCode() {
      return (int)value;
   }

   /**
    * Compares this object with the specified <tt>int</tt> for order.
    *
    * @param other   Value to compare with.
    * @return        A negative integer, zero, or a positive integer as
    *                this object is less than, equal to, or greater than
    *                the specified object.
    */
   public int compareTo(final int other) {
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
    * @throws ClassCastException    Object is not a <tt>HashCode</tt>.
    */
   public int compareTo(final Object obj) throws ClassCastException {
      HashCode hashCode = (HashCode)obj;
      return compareTo(hashCode.value);
   }

   /**
    * Test the equality of this <tt>HashCode</tt> and another object.
    *
    * @param obj    Object to test equality with.
    * @return       True if object is equal.
    */
   public boolean equals(final Object obj) {
      if (obj == this) return true;

      if (obj != null && obj.getClass() == getClass()) {
         return value == ((HashCode)obj).value;
      }

      return false;
   }

   /**
    * Return a string representation of this <tt>HashCode</tt>.
    *
    * @return  A string representation of this <tt>HashCode</tt>.
    */
   public String toString() {
      return String.valueOf(value);
   }

   /**
    * Return a cloned copy of this <tt>HashCode</tt>.
    *
    * @return   Cloned <tt>HashCode</tt>.
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
   //                           Generation Methods                        //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Generate a hash code for a boolean value.
    *
    * @param value   Boolean value to generate hash code from.
    * @return        Hash code.
    */
   public static int generate(final boolean value) {
      return value ? TRUE_HASHCODE : FALSE_HASHCODE;
   }

   /**
    * Generate a hash code for a long value.
    *
    * @param value   Long value to generate hash code from.
    * @return        Hash code.
    */
   public static int generate(final long value) {
      return (int)(value ^ (value >> 32));
   }

   /**
    * Generate a hash code for a double value.
    *
    * @param value   Double value to generate hash code from.
    * @return        Hash code.
    */
   public static int generate(final double value) {
      return generate(Double.doubleToLongBits(value));
   }

   /**
    * Generate a hash code for a float value.
    *
    * @param value   Float value to generate hash code from.
    * @return        Hash code.
    */
   public static int generate(final float value) {
      return Float.floatToIntBits(value);
   }

   /**
    * Generate a hash code for a byte array.
    *
    * @param bytes   An array of bytes to generate a hash code from.
    * @return        Hash code.
    */
   public static int generate(final byte[] bytes) {
      int hashcode = 0;

      for (int i=0; i<bytes.length; i++) {
         hashcode <<= 1;
         hashcode ^= bytes[i];
      } 

      return hashcode;
   }

   /**
    * Generate a hash code for an object array.
    *
    * <p>Does not handle nested primitive array elements.
    *
    * @param array   Array to generate hashcode for.
    * @param deep    True to traverse elements which are arrays to 
    *                determine the elements hash code.
    * @return        Hash code.
    */
   public static int generate(final Object array[], final boolean deep) {
      int hashcode = 0;

      for (int i=0; i<array.length; i++) {
         if (deep && (array[i] instanceof Object[])) {
            hashcode ^= generate((Object[])array[i], true);
         }
         else {
            hashcode ^= array[i].hashCode();
         }
      } 

      return hashcode;
   }

   /**
    * Generate a shallow hash code for an object array.
    *
    * @param array   Array to generate hashcode for.
    * @return        Hash code.
    */
   public static int generate(final Object array[]) {
      return generate(array, false);
   }

   /**
    * Generate a hash code for an object.
    *
    * @param obj     Object to generate hashcode for.
    * @return        Hash code.
    */
   public static int generate(final Object obj) {
      if (obj != null) {
         return obj.hashCode();
      }

      return NULL_HASHCODE;
   }
}
