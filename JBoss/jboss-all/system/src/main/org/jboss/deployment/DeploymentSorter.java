/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.deployment;

import java.util.Set;
import java.util.HashSet;
import java.util.List;
import java.util.ArrayList;
import java.util.Iterator;

import java.net.URL;

import org.jboss.util.NullArgumentException;
import java.util.Comparator;
import java.util.Collection;
import java.util.Collections;

/**
 * A helper class for sorting deployments.
 *
 * @version <tt>$Revision: 1.5.2.2 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class DeploymentSorter implements Comparator
{
   /** The default order for sorting deployments. */
   public static final String[] DEFAULT_SUFFIX_ORDER = {
       "sar", "rar", "ds.xml", "service.xml", "jar", "war", "wsr", "ear", "zip", "last" 
   };

   protected String[] suffixOrder;

   public DeploymentSorter(String[] suffixOrder)
   {
      if (suffixOrder == null)
         throw new NullArgumentException("suffixOrder");

      this.suffixOrder = suffixOrder;
   }

   public DeploymentSorter()
   {
      this(DEFAULT_SUFFIX_ORDER);
   }
   
   /**
    * Return a negetive number if o1 appears lower in the the suffixOrder than
    * o2.  For backward compatibility, this sort method supports both URLs 
    * and DeploymentInfo objects
    */
   public int compare(Object o1, Object o2) 
   {
      return getExtensionIndex((URL)o1) - getExtensionIndex((URL)o2);
   }
   
   /**
    * Return the index that matches this url
    */
   public int getExtensionIndex(URL url)
   {
      String path = url.getPath();
      if (path.endsWith("/")) 
          path = path.substring(0, path.length() - 1);
      int i = 0;
      for (; i < suffixOrder.length; i++)
      {
          if (path.endsWith(suffixOrder[i]))
              break;
      }
      return i;
   }
}
