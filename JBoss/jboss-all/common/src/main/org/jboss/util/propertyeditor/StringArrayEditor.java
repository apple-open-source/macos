/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.propertyeditor;

import java.util.ArrayList;
import java.util.StringTokenizer;
import java.beans.PropertyEditorSupport;

/**
 * A property editor for String[].
 *
 * @version <tt>$Revision: 1.1.4.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author Scott.Stark@jboss.org
 */
public class StringArrayEditor
   extends PropertyEditorSupport
{
   /** Build a String[]
    *
    */
   public void setAsText(final String text)
   {
      StringTokenizer stok = new StringTokenizer(text, ",");
      ArrayList list = new ArrayList();
      while (stok.hasMoreTokens())
      {
         list.add(stok.nextToken());
      }

      String[] theValue = new String[list.size()];
      list.toArray(theValue);
      setValue(theValue);
   }

   /**
    * @return a comma seperated string of the array elements
    */
   public String getAsText()
   {
      String[] theValue = (String[]) getValue();
      StringBuffer text = new StringBuffer();
      int length = theValue == null ? 0 : theValue.length;
      for(int n = 0; n < length; n ++)
      {
         text.append(theValue[n]);
         text.append(',');
      }
      // Remove the trailing ','
      text.setLength(text.length()-1);
      return text.toString();
   }
}
