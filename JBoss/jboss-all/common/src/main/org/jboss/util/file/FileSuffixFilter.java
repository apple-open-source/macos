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
 * A <em>suffix</em> based file filter.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class FileSuffixFilter
   implements FileFilter
{
   /** A list of suffixes which files must have to be accepted. */
   protected final String suffixes[];

   /** Flag to signal that we want to ignore the case. */
   protected final boolean ignoreCase;

   /**
    * Construct a <tt>FileSuffixFilter</tt>.
    *
    * @param suffixes   A list of suffixes which files mut have to be accepted.
    * @param ignoreCase <tt>True</tt> if the filter should be case-insensitive.
    */
   public FileSuffixFilter(final String suffixes[],
                           final boolean ignoreCase)
   {
      this.ignoreCase = ignoreCase;
      if (ignoreCase) {
         this.suffixes = new String[suffixes.length];
         for (int i=0; i<suffixes.length; i++) {
            this.suffixes[i] = suffixes[i].toLowerCase();
         }
      }
      else {
         this.suffixes = suffixes;
      }
   }

   /**
    * Construct a <tt>FileSuffixFilter</tt>.
    *
    * @param suffixes   A list of suffixes which files mut have to be accepted.
    */
   public FileSuffixFilter(final String suffixes[])
   {
      this(suffixes, false);
   }

   /**
    * Construct a <tt>FileSuffixFilter</tt>.
    *
    * @param suffix     The suffix which files must have to be accepted.
    * @param ignoreCase <tt>True</tt> if the filter should be case-insensitive.
    */
   public FileSuffixFilter(final String suffix,
                           final boolean ignoreCase)
   {
      this(new String[] { suffix }, ignoreCase);
   }

   /**
    * Construct a case sensitive <tt>FileSuffixFilter</tt>.
    *
    * @param suffix  The suffix which files must have to be accepted.
    */
   public FileSuffixFilter(final String suffix) {
      this(suffix, false);
   }

   /**
    * Check if a file is acceptible.
    *
    * @param file    The file to check.
    * @return        <tt>true</tt> if the file is acceptable.
    */
   public boolean accept(final File file) {
      boolean success = false;

      for (int i=0; i<suffixes.length && !success; i++) {
         if (ignoreCase)
            success = file.getName().toLowerCase().endsWith(suffixes[i]);
         else
            success = file.getName().endsWith(suffixes[i]);
      }

      return success;
   }
}
