/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.propertyeditor;

import java.util.Properties;

import java.io.ByteArrayInputStream;
import java.io.IOException;

import org.jboss.util.NestedRuntimeException;

/**
 * A property editor for {@link java.util.Properties}.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class PropertiesEditor
   extends TextPropertyEditorSupport
{
   /**
    * Returns a Properties object initialized with the input object
    * as a properties file based string.
    *
    * @return a Properties object
    *
    * @throws NestedRuntimeException  An IOException occured.
    */
   public Object getValue()
   {
      try {
         ByteArrayInputStream is = new ByteArrayInputStream(getAsText().getBytes());
         Properties p = new Properties();
         p.load(is);
      
         return p;
      }
      catch (IOException e) {
         throw new NestedRuntimeException(e);
      }
   }
}
