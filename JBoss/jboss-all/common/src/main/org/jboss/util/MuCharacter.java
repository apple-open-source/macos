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
 * A mutable character class.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class MuCharacter
   implements Comparable, Cloneable, Serializable, Mutable
{
   /** <code>char</code> value */
   private char value = 0;

   /**
    * Construct a new mutable character.
    */
   public MuCharacter() {}

   /**
    * Construct a new mutable character.
    *
    * @param c    <code>char</code> value.
    */
   public MuCharacter(char c) {
      value = c;
   }

   /**
    * Construct a new mutable character.
    *
    * @param obj  Object to convert to a <code>char</code>.
    */
   public MuCharacter(Object obj) {
      setValue(obj);
   }

   /**
    * Set the value.
    *
    * @param c    <code>char</code> value.
    * @return     The previous value.
    */
   public char set(char c) {
      char old = value;
      value = c;
      return old;
   }

   /**
    * Get the current value.
    *
    * @return     The current value.
    */
   public char get() {
      return value;
   }

   /**
    * Return the <code>char</code> value of this mutable character.
    *
    * @return   <code>char</code> value.
    */
   public char charValue() {
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
   public int compareTo(char other) {
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
    * @throws ClassCastException    Object is not a MuCharacter.
    */
   public int compareTo(Object obj) {
      return compareTo((MuCharacter)obj);
   }

   /**
    * Convert this mutable character to a string.
    *
    * @return   String value.
    */
   public String toString() {
      return String.valueOf(value);
   }

   /**
    * Get the hash code of this mutable character.
    *
    * @return   Hash code.
    */
   public int hashCode() {
      return value;
   }

   /**
    * Test the equality of this mutable character and another object.
    *
    * @param obj    Qbject to test equality with.
    * @return       True if object is equal.
    */
   public boolean equals(Object obj) {
      if (obj == this) return true;

      if (obj != null && obj.getClass() == getClass()) {
         return value == ((MuCharacter)obj).charValue();
      }

      return false;
   }

   /**
    * Return a cloned copy of this mutable character.
    *
    * @return   Cloned mutable character.
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
   //                            Mutable Support                          //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Set the value of this mutable character.
    *
    * @param obj  Object to convert to a <code>char</code>.
    *
    * @throws NotCoercibleException    Can not convert to <code>char</code>.
    */
   public void setValue(Object obj) {
      if (obj instanceof MuCharacter) {
         value = ((MuCharacter)obj).value;
      }
      else if (obj instanceof Character) {
         value = ((Character)obj).charValue();
      }
      else if (obj instanceof Number) {
         value = (char)((Number)obj).intValue();
      }
      else {
         throw new NotCoercibleException("can not convert to 'char': " + obj);
      }
   }

   /**
    * Return the char value of this mutable character.
    *
    * @return   <code>java.lang.Character</code> value.
    */
   public Object getValue() {
      return new Character(value);
   }
}

