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
 * An abstract mutable number class.
 *
 * <p>This is a base wrapper class for <code>java.lang.Number</code>.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public abstract class MuNumber 
   extends Number
   implements Comparable, Cloneable, Mutable
{
   /**
    * Returns the value of the specified number as a <code>byte</code>.
    * This may involve rounding or truncation.
    *
    * @return  The numeric value represented by this object after conversion
    *          to type <code>byte</code>.
    */
   public byte byteValue() {
      return (byte)longValue();
   } 

   /**
    * Returns the value of the specified number as a <code>short</code>.
    * This may involve rounding or truncation.
    *
    * @return  The numeric value represented by this object after conversion
    *          to type <code>short</code>.
    */
   public short shortValue() {
      return (short)longValue();
   }

   /**
    * Returns the value of the specified number as a <code>int</code>.
    * This may involve rounding or truncation.
    *
    * @return  The numeric value represented by this object after conversion
    *          to type <code>int</code>.
    */
   public int intValue() {
      return (int)longValue();
   }

   /**
    * Returns the value of the specified number as a <code>float</code>.
    * This may involve rounding or truncation.
    *
    * @return  The numeric value represented by this object after conversion
    *          to type <code>float</code>.
    */
   public float floatValue() {
      return (float)doubleValue();
   }
}
