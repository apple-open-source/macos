/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.net.protocol;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Collection;
import java.util.StringTokenizer;
import java.net.URL;
import java.io.IOException;

/**
 * Support class for URLLister's providing protocol independent functionality.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.2.2.3 $
 */
public abstract class URLListerBase implements URLLister
{
   public Collection listMembers (URL baseUrl, String patterns,
      boolean scanNonDottedSubDirs) throws IOException
   {
      // @todo, externalize the separator?
      StringTokenizer tokens = new StringTokenizer (patterns, ",");
      String[] members = new String[tokens.countTokens ()];
      for (int i=0; tokens.hasMoreTokens (); i++)
      {
         String token = tokens.nextToken ();
         // Trim leading/trailing spaces as its unlikely they are meaningful
         members[i] = token.trim();
      }
      URLFilter filter = new URLFilterImpl (members);
      return listMembers (baseUrl, filter, scanNonDottedSubDirs);
   }

   public Collection listMembers (URL baseUrl, String patterns) throws IOException
   {
      return listMembers (baseUrl, patterns, false);
   }
   
   /**
    * Inner class representing Filter criteria to be applied to the members
    * of the returned Collection
    */
   public static class URLFilterImpl implements URLFilter
   {
      protected boolean allowAll;
      protected HashSet constants;
      
      public URLFilterImpl (String[] patterns)
      {
         constants = new HashSet (Arrays.asList (patterns));
         allowAll = constants.contains ("*");
      }
      
      public boolean accept (URL baseUrl, String name)
      {
         if (allowAll)
         {
            return true;
         }
         if (constants.contains (name))
         {
            return true;
         }
         return false;
      }
   }
   
   protected static final URLFilter acceptAllFilter = new URLFilter ()
   {
      public boolean accept (URL baseURL, String memberName)
      {
         return true;
      }
   };
}
