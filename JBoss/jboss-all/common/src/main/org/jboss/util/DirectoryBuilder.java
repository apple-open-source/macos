/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.util;

import java.io.File;

/**
 * A simple utility to make it easier to build File objects for nested
 * directories based on the command line 'cd' pattern.
 *      
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @version $Revision: 1.1 $
 */
public class DirectoryBuilder
{
   protected File root;

   public DirectoryBuilder() {
      // empty
   } 

   public DirectoryBuilder(final File root) {
      this.root = root;
   } 

   public DirectoryBuilder(final File root, final File child) {
      this(root);
      cd(child);
   }

   public DirectoryBuilder(final String rootname) {
      this(new File(rootname));
   }

   public DirectoryBuilder(final String rootname, final String childname) {
      this(new File(rootname), new File(childname));
   }

   public DirectoryBuilder cd(final File child) {
      if (child.isAbsolute()) {
	 root = child;
      }
      else {
	 root = new File(root, child.getPath());
      }
      return this;
   }

   public DirectoryBuilder cd(final String childname) {
      return cd(new File(childname));
   }

   public File get() {
      return root;
   }

   public String toString() {
      return root.toString();
   }
}
