/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.net.protocol.file;

import java.io.IOException;

import java.net.URL;
import java.net.URLConnection;
import java.net.URLStreamHandler;

/**
 * A protocol handler for the 'file' protocol.
 *
 * @version <tt>$Revision: 1.2 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class Handler
   extends URLStreamHandler
{
   public URLConnection openConnection(final URL url)
      throws IOException
   {
      return new FileURLConnection(url);
   }

   protected void parseURL(final URL url, final String s, final int i, final int j)
   {
      super.parseURL(url, s.replace(java.io.File.separatorChar, '/'), i, j);
   }
}
