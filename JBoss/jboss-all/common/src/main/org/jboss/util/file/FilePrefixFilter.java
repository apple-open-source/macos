/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.file;

import java.io.File;
import java.io.FileFilter;

/**
 * A <em>prefix</em> based file filter.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class FilePrefixFilter
   implements FileFilter
{
   /** The prefix which files must have to be accepted. */
   protected final String prefix;

   /** Flag to signal that we want to ignore the case. */
   protected final boolean ignoreCase;

   /**
    * Construct a <tt>FilePrefixFilter</tt>.
    *
    * @param prefix     The prefix which files must have to be accepted.
    * @param ignoreCase <tt>True</tt> if the filter should be case-insensitive.
    */
   public FilePrefixFilter(final String prefix,
                           final boolean ignoreCase)
   {
      this.ignoreCase = ignoreCase;
      this.prefix = (ignoreCase ? prefix.toLowerCase() : prefix);
   }

   /**
    * Construct a case sensitive <tt>FilePrefixFilter</tt>.
    *
    * @param prefix  The prefix which files must have to be accepted.
    */
   public FilePrefixFilter(final String prefix) {
      this(prefix, false);
   }

   /**
    * Check if a file is acceptible.
    *
    * @param file    The file to check.
    * @return        <tt>true</tt> if the file is acceptable.
    */
   public boolean accept(final File file) {
      if (ignoreCase) {
         return file.getName().toLowerCase().startsWith(prefix);
      }
      else {
         return file.getName().startsWith(prefix);
      }
   }
}
