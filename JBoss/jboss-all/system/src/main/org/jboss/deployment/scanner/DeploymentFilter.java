/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.deployment.scanner;

import java.io.FileFilter;
import java.io.File;
import java.util.Arrays;
import java.util.Comparator;
import java.util.ArrayList;
import java.util.Collections;
import java.net.URL;

import org.jboss.net.protocol.URLLister;

/**
 * <p>A simple filter to for the URLDeploymentScanner.  Three arrays are
 * maintained for checking: a prefix, suffix, and match array.  If the
 * filename starts with any of the prefixes, ends with any of the
 * suffixes, or exactly matches any of the matches, then the accepts
 * method will return false.
 */
public class DeploymentFilter implements FileFilter, URLLister.URLFilter
{
   /**
    * Compare the strings backwards.  This assists in suffix comparisons.
    */
   private static final Comparator reverseComparator = new Comparator()
   {
      public int compare(Object o1, Object o2)
      {
         int idx1 = ((String) o1).length();
         int idx2 = ((String) o2).length();
         int comp = 0;

         while (comp == 0 && idx1 > 0 && idx2 > 0)
            comp = ((String) o1).charAt(--idx1) - ((String) o2).charAt(--idx2);

         return (comp == 0) ? (idx1 - idx2) : comp;
      }
   };

   /** the default prefix list */
   private static final String[] DEFAULT_PREFIXES =
      {"#", "%", ",", ".", "_$"};

   /** the default suffix list */
   private static final String[] DEFAULT_SUFFIXES =
      {"#", "$", "%", ".BAK", ".old", ".orig", ".rej", ".bak", ".sh",
          ",v", "~"};

   /** the default matches list */
   private static final String[] DEFAULT_MATCHES =
      {".make.state", ".nse_depinfo", "CVS", "CVS.admin", "RCS", "RCSLOG",
          "SCCS", "TAGS", "core", "tags"};

   /** The list of disallowed suffixes, sorted using reverse values */
   private ArrayList suffixes;

   /** The sorted list of disallowed prefixes */
   private ArrayList prefixes;

   /** The sorted list of disallowed values */
   private ArrayList matches;

   /** Use the default values for suffixes, prefixes, and matches */
   public DeploymentFilter()
   {
      this(DEFAULT_MATCHES, DEFAULT_PREFIXES, DEFAULT_SUFFIXES);
   }

   /**
    * Create using a custom set of matches, prefixes, and suffixes.  If any of
    * these arrays are null, then the corresponding default will be
    * substituted.
    */
   public DeploymentFilter(String[] matches, String[] prefixes, String[] suffixes)
   {
      if( matches == null )
         matches = DEFAULT_MATCHES;
      Arrays.sort(matches);
      this.matches = new ArrayList(Arrays.asList(matches));

      if( prefixes == null )
         prefixes = DEFAULT_PREFIXES;
      Arrays.sort(prefixes);
      this.prefixes = new ArrayList(Arrays.asList(prefixes));

      if( suffixes == null )
         suffixes = DEFAULT_SUFFIXES;
      Arrays.sort(suffixes, reverseComparator);
      this.suffixes = new ArrayList(Arrays.asList(suffixes));
   }

   public void addPrefix(String prefix)
   {
      this.prefixes.add(prefix);
      Collections.sort(this.prefixes);
   }
   public void addPrefixes(String[] prefixes)
   {
      this.prefixes.add(Arrays.asList(prefixes));
      Collections.sort(this.prefixes);
   }

   public void delPrefix(String prefix)
   {
      this.prefixes.remove(prefix);
   }
   public void delPrefixes(String[] prefixes)
   {
      this.prefixes.removeAll(Arrays.asList(prefixes));
      Collections.sort(this.prefixes);
   }

   public void addSuffix(String suffix)
   {
      this.suffixes.add(suffix);
      Collections.sort(this.suffixes, reverseComparator);
   }
   public void addSuffixes(String[] suffixes)
   {
      this.suffixes.add(Arrays.asList(suffixes));
      Collections.sort(this.suffixes, reverseComparator);
   }

   public void delSuffix(String suffix)
   {
      this.suffixes.remove(suffix);
   }
   public void delSuffixes(String[] suffixes)
   {
      this.suffixes.removeAll(Arrays.asList(suffixes));
      Collections.sort(this.suffixes, reverseComparator);
   }

   /**
    * If the filename matches any string in the prefix, suffix, or matches
    * array, return false.  Perhaps a bit of overkill, but this method
    * operates in log(n) time, where n is the size of the arrays.
    *
    * @param  file  The file to be tested
    * @return  <code>false</code> if the filename matches any of the prefixes,
    *          suffixes, or matches.
    */
   public boolean accept(File file)
   {
      return accept(file.getName());
   }

   public boolean accept(URL baseURL, String memberName)
   {
      return accept(memberName);
   }

   private boolean accept(String name)
   {
      // check exact match
      int index = Collections.binarySearch(matches, name);
      if (index >= 0)
         return false;

      // check prefix
      index = Collections.binarySearch(prefixes, name);
      if (index >= 0)
         return false;
      if (index < -1)
      {
         // The < 0 index gives the first index greater than name
         int firstLessIndex = -2 - index;
         String prefix = (String) prefixes.get(firstLessIndex);
         // If name starts with an ingored prefix ignore name
         if( name.startsWith(prefix) )
            return false;
      }

      // check suffix
      index = Collections.binarySearch(suffixes, name, reverseComparator);
      if (index >= 0)
         return false;
      if (index < -1)
      {
         // The < 0 index gives the first index greater than name
         int firstLessIndex = -2 - index;
         String suffix = (String) suffixes.get(firstLessIndex);
         // If name ends with an ingored suffix ignore name
         if( name.endsWith(suffix) )
            return false;
      }

      // everything checks out.
      return true;
   }
}
