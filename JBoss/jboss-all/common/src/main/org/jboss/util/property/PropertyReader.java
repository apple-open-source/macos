/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.property;

import java.util.Map;

import java.io.IOException;

/**
 * Iterface used to allow a <tt>PropertyMap</tt> to read property definitions 
 * in an implementation independent fashion.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public interface PropertyReader
{
   /**
    * Read a map of properties from this input source.
    *
    * @return  Read properties map.
    *
    * @throws PropertyException    Failed to read properties.
    * @throws IOException          I/O error while reading properties.
    */
   Map readProperties() throws PropertyException, IOException;
}
