/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.propertyeditor;

import java.beans.PropertyEditorSupport;

/**
 * A property editor support class for textual properties.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class TextPropertyEditorSupport
   extends PropertyEditorSupport
{
   protected TextPropertyEditorSupport(final Object source)
   {
      super(source);
   }

   protected TextPropertyEditorSupport()
   {
      super();
   }
   
   /**
    * Sets the property value by parsing a given String.
    *
    * @param text  The string to be parsed.
    */
   public void setAsText(final String text)
   {
      setValue(text);
   }
}
