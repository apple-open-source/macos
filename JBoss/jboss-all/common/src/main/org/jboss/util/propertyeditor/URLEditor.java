/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.propertyeditor;

import java.net.MalformedURLException;

import org.jboss.util.Strings;

import org.jboss.util.NestedRuntimeException;

/**
 * A property editor for {@link java.net.URL}.
 *
 * @version <tt>$Revision: 1.1.4.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class URLEditor
   extends TextPropertyEditorSupport
{
   /**
    * Returns a URL for the input object converted to a string.
    *
    * @return a URL object
    *
    * @throws NestedRuntimeException   An MalformedURLException occured.
    */
   public Object getValue()
   {
      try {
         return Strings.toURL(getAsText());
      }
      catch (MalformedURLException e) {
         throw new NestedRuntimeException(e);
      }
   }
}
