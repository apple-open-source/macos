/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.propertyeditor;

import org.jboss.util.NestedRuntimeException;

/**
 * A property editor for {@link java.lang.Class}.
 *
 * @version <tt>$Revision: 1.1.4.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class ClassEditor
   extends TextPropertyEditorSupport
{
   /**
    * Returns a Class for the input object converted to a string.
    *
    * @return a Class object
    *
    * @throws NestedRuntimeException   Failed to create Class instance.
    */
   public Object getValue()
   {
      try
      {
         ClassLoader loader = Thread.currentThread().getContextClassLoader();
         String classname = getAsText();
         Class type = loader.loadClass(classname);

         return type;
      }
      catch (Exception e)
      {
         throw new NestedRuntimeException(e);
      }
   }
}
