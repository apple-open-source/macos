/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.util.test;

import java.io.File;
import java.net.URL;
import java.net.URLConnection;

import org.jboss.test.JBossTestCase;
import org.jboss.net.protocol.URLStreamHandlerFactory;
import org.jboss.net.protocol.resource.ResourceURLConnection;
import org.jboss.net.protocol.file.FileURLConnection;

/** Unit tests for the custom JBoss protocol handler

@see org.jboss.net.protocol.URLStreamHandlerFactory
@author Scott.Stark@jboss.org
@version $Revision: 1.1.2.1 $
**/
public class ProtocolHandlerUnitTestCase extends JBossTestCase
{
   public ProtocolHandlerUnitTestCase(String name)
   {
      super(name);
   }

   public void testJBossHandlers()
      throws Exception
   {
      getLog().debug("+++ testJBossHandlers");
      // Install a URLStreamHandlerFactory that uses the TCL
      URL.setURLStreamHandlerFactory(new URLStreamHandlerFactory());
      File cwd = new File(".");
      URL cwdURL = cwd.toURL();
      URLConnection conn = cwdURL.openConnection();
      getLog().debug("File URLConnection: "+conn);
      assertTrue("URLConnection is JBoss FileURLConnection", conn instanceof FileURLConnection);
      long lastModified = conn.getLastModified();
      getLog().debug("CWD lastModified: "+lastModified);
      assertTrue("CWD lastModified != 0", lastModified != 0);

      URL resURL = new URL("resource:log4j.xml");
      conn = resURL.openConnection();
      getLog().debug("log4j.xml URLConnection: "+conn);
      assertTrue("URLConnection is JBoss ResourceURLConnection", conn instanceof ResourceURLConnection);
      lastModified = conn.getLastModified();
      getLog().debug("log4j.xml lastModified: "+lastModified);
      assertTrue("log4j.xml lastModified != 0", lastModified != 0);
   }

   /** Override the testServerFound since these test don't need the JBoss server
    */
   public void testServerFound()
   {
   }

}

