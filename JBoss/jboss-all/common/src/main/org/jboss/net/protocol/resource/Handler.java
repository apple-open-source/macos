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

import java.net.URL;
import java.net.URLConnection;
import java.net.URLStreamHandler;

/**
 * A protocol handler for the 'resource' protocol.  Provides
 * access to system resources.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class Handler
   extends URLStreamHandler
{
   public URLConnection openConnection(final URL url)
      throws IOException
   {
      return new ResourceURLConnection(url);
   }
}
