/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.mx.util.propertyeditor;

import javax.management.ObjectName;
import javax.management.MalformedObjectNameException;

import org.jboss.util.NestedRuntimeException;
import org.jboss.util.propertyeditor.TextPropertyEditorSupport;

/**
 * A property editor for {@link javax.management.ObjectName}.
 *
 * @version <tt>$Revision: 1.1.2.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class ObjectNameEditor
   extends TextPropertyEditorSupport
{
   /**
    * Returns a ObjectName for the input object converted to a string.
    *
    * @return a ObjectName object
    *
    * @throws org.jboss.util.NestedRuntimeException   An MalformedObjectNameException occured.
    */
   public Object getValue()
   {
      try {
         return new ObjectName(getAsText());
      }
      catch (MalformedObjectNameException e) {
         throw new NestedRuntimeException(e);
      }
   }
}
