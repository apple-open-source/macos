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
 * A <tt>java.lang.Class</tt> coercion handler.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class ClassHandler
   extends BoundCoercionHandler
{
   /**
    * Get the target class type for this <tt>CoercionHandler</tt>.
    *
    * @return     Class type.
    */
   public Class getType() {
      return Class.class;
   }

   /**
    * Coerces the given value into the given type (which should be
    * <tt>Class</tt>).
    *
    * <p>This currently only support coercion from a <tt>String</tt>.
    *
    * @param value   Value to coerce.
    * @param type    <tt>java.lang.Class</tt>.
    * @return        Value coerced into a <tt>Class</tt>.
    *
    * @throws CoercionException  Failed to coerce.
    */
   public Object coerce(Object value, Class type) throws CoercionException {
      if (value.getClass().equals(String.class)) {
         return coerce((String)value);
      }
      
      throw new NotCoercibleException(value);
   }

   /**
    * Coerces the given String into a <tt>Class</tt> by doing a
    * <code>Class.forName()</code>.
    *
    * @param value   String value to convert to a <tt>Class</tt>.
    * @return        <tt>Class</tt> value.
    *
    * @throws NotCoercibleException    Class not found.
    */
   public Object coerce(String value) {
      try {
         return Class.forName(value);
      }
      catch (ClassNotFoundException e) {
         throw new NotCoercibleException(value, e);
      }
   }
}

