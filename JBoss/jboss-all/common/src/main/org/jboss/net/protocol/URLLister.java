/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.net.protocol;

import java.io.IOException;
import java.net.URL;
import java.util.Collection;

/**
 * Interface defining methods that can be used to list the contents of a URL
 * collection irrespective of the protocol.
 */
public interface URLLister {
   /**
    * List the members of the given collection URL that match the patterns
    * supplied and, if it contains directory that contains NO dot in the name and
    * scanNonDottedSubDirs is true, recursively finds URL in these directories.
    * @param baseUrl the URL to list; must end in "/"
    * @param patterns the patterns to match (separated by ',')
    * @param scanNonDottedSubDirs enables recursive search for directories containing no dots
    * @return a Collection of URLs that match
    * @throws IOException if there was a problem getting the list
    */
   Collection listMembers(URL baseUrl, String patterns, boolean scanNonDottedSubDirs) throws IOException;
   
   /**
    * List the members of the given collection URL that match the patterns
    * supplied. Doesn't recursively list files contained in directories.
    * @param baseUrl the URL to list; must end in "/"
    * @param patterns the patterns to match (separated by ',')
    * @return a Collection of URLs that match
    * @throws IOException if there was a problem getting the list
    */
   Collection listMembers(URL baseUrl, String patterns) throws IOException;

   /**
    * List the members of the given collection that are accepted by the filter
    * @param baseUrl the URL to list; must end in "/"
    * @param filter a filter that is called to determine if a member should
    *               be returned
    * @param scanNonDottedSubDirs enables recursive search for directories containing no dots
    * @return a Collection of URLs that match
    * @throws IOException if there was a problem getting the list
    */
   Collection listMembers(URL baseUrl, URLFilter filter, boolean scanNonDottedSubDirs) throws IOException;

   /**
    * List the members of the given collection that are accepted by the filter
    * @param baseUrl the URL to list; must end in "/"
    * @param filter a filter that is called to determine if a member should
    *               be returned
    * @return a Collection of URLs that match
    * @throws IOException if there was a problem getting the list
    */
   Collection listMembers(URL baseUrl, URLFilter filter) throws IOException;

   /**
    * Interface defining a filter for listed members.
    */
   public interface URLFilter {
      /**
       * Determine whether the supplied memberName should be accepted
       * @param baseURL the URL of the collection
       * @param memberName the member of the collection
       * @return
       */
      boolean accept(URL baseURL, String memberName);
   }
}
