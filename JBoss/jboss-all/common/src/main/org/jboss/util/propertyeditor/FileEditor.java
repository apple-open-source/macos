/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.propertyeditor;

import java.io.File;
import java.io.IOException;

import org.jboss.util.NestedRuntimeException;

/**
 * A property editor for {@link java.io.File}.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class FileEditor
   extends TextPropertyEditorSupport
{
   /**
    * Returns a URL for the input object converted to a string.
    *
    * @return a URL object
    *
    * @throws NestedRuntimeException   An IOException occured.
    */
   public Object getValue()
   {
      try {
         return new File(getAsText()).getCanonicalFile();
      }
      catch (IOException e) {
         throw new NestedRuntimeException(e);
      }
   }
}
