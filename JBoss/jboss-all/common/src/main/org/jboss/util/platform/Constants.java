/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.platform;

import org.jboss.util.property.Property;

/**
 * Platform constants.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public interface Constants
{
   /** Platform dependent line separator. */
   String LINE_SEPARATOR = Property.get("line.separator");

   /** Platform dependant file separator. */
   String FILE_SEPARATOR = Property.get("file.separator");

   /** Platform dependant path separator. */
   String PATH_SEPARATOR = Property.get("path.separator");
}
