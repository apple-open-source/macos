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

/** A property editor for {@link java.lang.Boolean}.
 *
 * @version $Revision: 1.2 $
 * @author Scott.Stark@jboss.org
 */
public class BooleanEditor extends PropertyEditorSupport
{
   private static final String[] BOOLEAN_TAGS = {"true", "false"};

   /** Map the argument text into Boolean.TRUE or Boolean.FALSE
    using Boolean.valueOf.
    */
   public void setAsText(final String text)
   {
      Object newValue = Boolean.valueOf(text);
      setValue(newValue);
   }

   /**
    @return the values {"true", "false"}
    */
   public String[] getTags()
   {
      return BOOLEAN_TAGS;
   }
}
