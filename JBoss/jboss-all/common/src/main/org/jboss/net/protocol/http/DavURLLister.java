/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/


package org.jboss.net.protocol.http;

import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

import org.apache.commons.httpclient.HttpException;
import org.apache.util.HttpURL;
import org.apache.webdav.lib.WebdavResource;
import org.jboss.net.protocol.URLListerBase;

public class DavURLLister extends URLListerBase
{
   public Collection listMembers (URL baseUrl, URLFilter filter) throws IOException
   {
      return listMembers (baseUrl, filter, false);
   }

   public Collection listMembers (URL baseUrl, URLFilter filter, boolean scanNonDottedSubDirs) throws IOException
   {
      WebdavResource resource = null;
      try
      {
         resource = new WebdavResource (baseUrl.toString ());
         WebdavResource[] resources = resource.listWebdavResources ();
         List urls = new ArrayList (resources.length);
         for (int i = 0; i < resources.length; i++)
         {
            WebdavResource member = resources[i];
            HttpURL httpURL = member.getHttpURL ();
            if (filter.accept (baseUrl, httpURL.getName ()))
            {
               String url = httpURL.getUnescapedHttpURL ();
               if (member.isCollection ())
               {
                  if (! url.endsWith ("/"))
                     url += "/";

                  // it is a directory: do we have to recursively list its content?
                  //
                  if (scanNonDottedSubDirs && getFilePartFromUrl (httpURL.toURL ()).indexOf (".") == -1)
                  {
                     URL subUrl = new URL (url) ;
                     urls.addAll (listMembers (subUrl, filter, scanNonDottedSubDirs));
                  }
                  else
                  {
                     urls.add (new URL (url));
                  }
               }
               else
               {
                  urls.add (new URL (url));
               }
               
            }
         }
         return urls;
      } catch (HttpException e)
      {
         throw new IOException (e.getMessage ());
      } catch (MalformedURLException e)
      {
         // should not happen
         throw new IllegalStateException (e.getMessage ());
      } finally
      {
         if (resource != null)
         {
            resource.close ();
         }
      }
   }
   
   protected static final String getFilePartFromUrl (URL source)
   {
      String name = source.getPath ();
      int length = name.length ();
      
      if (name.charAt (length - 1) == '/')
      {
         int start = name.lastIndexOf ("/", length - 2);
         return name.substring (start, length -2);
      }
      else
      {
         int start = name.lastIndexOf ("/");
         return name.substring (start);         
      }
   }
   
}
