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

import java.io.File;

/**
 * A <tt>java.io.File</tt> coercion handler.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class FileHandler
   extends BoundCoercionHandler
{
   /**
    * Get the target class type for this <tt>CoercionHandler</tt>.
    *
    * @return  <tt>Class</tt> type.
    */
   public Class getType() {
      return File.class;
   }

   /**
    * Coerces the given value into the given type (which should be
    * <tt>File</tt>).
    *
    * <p>This currently only support coercion from a <tt>String</tt>
    *
    * @param value   Value to coerce.
    * @param type    <tt>File</tt>.
    * @return        Value coerced into a <tt>File</tt>.
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
    * Coerces the given String into a <tt>File</tt> by creating attempting
    * to create a new file for the given filename.
    *
    * @param value   The name of the file to create.
    * @return        <tt>File</tt>
    *
    * @throws NotCoercibleException    Failed to create file.
    */
   public Object coerce(String value) {
      return new File(value);
   }
}

