/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.net.protocol.resource;

import java.io.IOException;
import java.io.FileNotFoundException;

import java.net.URL;
import java.net.MalformedURLException;

import org.jboss.net.protocol.DelegatingURLConnection;

import org.jboss.logging.Logger;

/**
 * Provides access to system resources as a URLConnection.
 *
 * @version <tt>$Revision: 1.3.2.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author Scott.Stark@jboss.org
 */
public class ResourceURLConnection
   extends DelegatingURLConnection
{
   private static final Logger log = Logger.getLogger(ResourceURLConnection.class);

   public ResourceURLConnection(final URL url)
      throws MalformedURLException, IOException
   {
      super(url);
   }

   protected URL makeDelegateUrl(final URL url)
      throws MalformedURLException, IOException
   {
      String name = url.getHost();
      String file = url.getFile();
      if (file != null && !file.equals(""))
      {
         name += file;
      }

      // first try TCL and then SCL

      ClassLoader cl = Thread.currentThread().getContextClassLoader();
      URL target = cl.getResource(name);

      if (target == null)
      {
         cl = ClassLoader.getSystemClassLoader();
         target = cl.getResource(name);
      }

      if (target == null)
         throw new FileNotFoundException("Could not locate resource: " + name);

      if (log.isTraceEnabled())
      {
         log.trace("Target resource URL: " + target);
         try
         {
            log.trace("Target resource URL connection: " + target.openConnection());
         }
         catch (Exception ignore)
         {
         }
      }

      // Return a new URL as the cl version does not use the JB stream factory
      return new URL(target.toExternalForm());
   }
}
