/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.propertyeditor;

import java.net.InetAddress;
import java.net.UnknownHostException;

import org.jboss.util.NestedRuntimeException;
import org.jboss.util.Strings;

/**
 * A property editor for {@link java.net.InetAddress}.
 *
 * @version <tt>$Revision: 1.6.2.1 $</tt>
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 */
public class InetAddressEditor
   extends TextPropertyEditorSupport
{
   /**
    * Returns a InetAddress for the input object converted to a string.
    *
    * @return an InetAddress
    *
    * @throws NestedRuntimeException   An UnknownHostException occured.
    */
   public Object getValue()
   {
      try
      {
         String text = getAsText();
         if (text == null)
         {
            return null;
         }
         if (text.startsWith("/"))
         {
            // seems like localhost sometimes will look like:
            // /127.0.0.1 and the getByNames barfs on the slash - JGH
            text = text.substring(1);
         }
         return InetAddress.getByName(Strings.replaceProperties(text));
      }
      catch (UnknownHostException e)
      {
         throw new NestedRuntimeException(e);
      }
   }
}
