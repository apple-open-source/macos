/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.coerce;

import org.jboss.util.CoercionException;
import org.jboss.util.NotCoercibleException;

/**
 * A Character coercion handler.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class CharacterHandler
   extends BoundCoercionHandler
{
   /**
    * Get the target class type for this CoercionHandler.
    *
    * @return     Class type
    */
   public Class getType() {
      return Character.class;
   }

   /**
    * Coerces the given value into the given type (which should be
    * Character.class).
    *
    * <p>This currently only support coercion from a String.
    *
    * @param value   Value to coerce
    * @param type    Character.class
    * @return        Value coerced into a Character
    *
    * @throws CoercionException  Failed to coerce
    */
   public Object coerce(Object value, Class type) throws CoercionException {
      if (value.getClass().equals(String.class)) {
         return coerce((String)value);
      }
      
      throw new NotCoercibleException(value);
   }

   /**
    * Coerces the given string into a Character, by taking off the first
    * index of the string and wrapping it.
    *
    * @param value   String value to convert to a Character
    * @return        Character value or null if the string is empty.
    */
   public Object coerce(String value) {
      char[] temp = value.toCharArray();
      if (temp.length == 0) {
         return null;
      }
      return new Character(temp[0]);
   }
}

