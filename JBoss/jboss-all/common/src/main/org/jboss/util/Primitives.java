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
 * Primitive utilities.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public final class Primitives
{
   /**
    * Test the equality of two doubles by converting their values into
    * IEEE 754 floating-point "double format" long values.
    *
    * @param a    Double to check equality with.
    * @param b    Double to check equality with.
    * @return     True if a equals b.
    */
   public static boolean equals(final double a, final double b) {
      return Double.doubleToLongBits(a) == Double.doubleToLongBits(b);
   }

   /**
    * Test the equality of two doubles by converting their values into
    * IEEE 754 floating-point "single precision" bit layouts.
    *
    * @param a    Float to check equality with.
    * @param b    Float to check equality with.
    * @return     True if a equals b.
    */
   public static boolean equals(final float a, final float b) {
      return Float.floatToIntBits(a) == Float.floatToIntBits(b);
   }

   /**
    * Test the equality of a given sub-section of two byte arrays.
    *
    * @param a       The first byte array.
    * @param abegin  The begining index of the first byte array.
    * @param b       The second byte array.
    * @param bbegin  The begining index of the second byte array.
    * @param length  The length of the sub-section.
    * @return        True if sub-sections are equal.
    */
   public static boolean equals(final byte a[], final int abegin,
                                final byte b[], final int bbegin,
                                final int length)
   {
      try {
         int i=length;
         while (--i >= 0) {
            if (a[abegin + i] != b[bbegin + i]) {
               return false;
            }
         }
      }
      catch (ArrayIndexOutOfBoundsException e) {
         return false;
      }

      return true;
   }

   /**
    * Test the equality of two byte arrays.
    *
    * @param a    The first byte array.
    * @param b    The second byte array.
    * @return     True if the byte arrays are equal.
    */
   public static boolean equals(final byte a[], final byte b[]) {
      if (a == b) return true;
      if (a == null || b == null) return false;
      if (a.length != b.length) return false;

      try {
         for (int i=0; i<a.length; i++) {
            if (a[i] != b[i]) {
               return false;
            }
         }
      }
      catch (ArrayIndexOutOfBoundsException e) {
         return false;
      }

      return true;
   }

   /**
    * Safely convert a <tt>long</tt> into a <tt>int</tt> value.
    *
    * <p>If value is > Integer.MAX_VALUE or < Integer.MIN_VALUE
    *    then an exception will be thrown, else the value is cast
    *    down to an <tt>int</tt>.
    *
    * @param value   The <tt>long</tt> value to convert.
    * @return        The converted value.
    *
    * @throws DataConversionException  Could not safely convert the value.
    */
   public static int toInt(final long value)
      throws DataConversionException
   {
      if (value > Integer.MAX_VALUE || value < Integer.MIN_VALUE)
         throw new DataConversionException
            ("can not safly convert to int: " + value);

      return (int)value;
   }
}
