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
import java.io.FilenameFilter;

/**
 * A <em>suffix</em> based filename filter.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class FilenameSuffixFilter
   implements FilenameFilter
{
   /** The suffix which files must have to be accepted. */
   protected final String suffix;

   /** Flag to signal that we want to ignore the case. */
   protected final boolean ignoreCase;

   /**
    * Construct a <tt>FilenameSuffixFilter</tt>.
    *
    * @param suffix     The suffix which files must have to be accepted.
    * @param ignoreCase <tt>True</tt> if the filter should be case-insensitive.
    */
   public FilenameSuffixFilter(final String suffix,
                               final boolean ignoreCase)
   {
      this.ignoreCase = ignoreCase;
      this.suffix = (ignoreCase ? suffix.toLowerCase() : suffix);
   }

   /**
    * Construct a case sensitive <tt>FilenameSuffixFilter</tt>.
    *
    * @param suffix  The suffix which files must have to be accepted.
    */
   public FilenameSuffixFilter(final String suffix) {
      this(suffix, false);
   }

   /**
    * Check if a file is acceptible.
    *
    * @param dir  The directory the file resides in.
    * @param name The name of the file.
    * @return     <tt>true</tt> if the file is acceptable.
    */
   public boolean accept(final File dir, final String name) {
      if (ignoreCase) {
         return name.toLowerCase().endsWith(suffix);
      }
      else {
         return name.endsWith(suffix);
      }
   }
}
