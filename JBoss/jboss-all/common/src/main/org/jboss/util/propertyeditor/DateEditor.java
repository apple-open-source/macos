/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.propertyeditor;

import java.text.DateFormat;
import java.text.ParseException;

import org.jboss.util.NestedRuntimeException;

/**
 * A property editor for {@link Date}.
 *
 * @version <tt>$Revision: 1.4 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author  <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 */
public class DateEditor
   extends TextPropertyEditorSupport
{
   /**
    * Returns a Date for the input object converted to a string.
    *
    * @return a Date object
    *
    */
   public Object getValue()
   {
      try 
      {
         DateFormat df = DateFormat.getDateInstance();
         return df.parse(getAsText());
      }
      catch (ParseException e)
      {
         throw new NestedRuntimeException(e);
      }
   }
}
