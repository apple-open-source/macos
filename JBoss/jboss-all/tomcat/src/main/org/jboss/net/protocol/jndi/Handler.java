/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.net.protocol.jndi;

import org.apache.naming.resources.DirContextURLConnection;
import org.apache.naming.resources.DirContextURLStreamHandler;

import javax.naming.directory.DirContext;
import java.io.IOException;

import java.net.URL;
import java.net.URLConnection;
import java.net.URLStreamHandler;

/**
 * A protocol handler for the 'jndi' protocol.  Provides
 * access to jndi resources required by Tomcat 4.1.12. This
 * is basically a place-marker class so the org.jboss.net.protocol.URLStreamHandlerFactory
 * class can find org.apache.naming.resources.DirContextURLStreamHandler.
 * See org.jboss.net.protocol.URLStreamHandlerFactory and org.jboss.net.protocol.file.Handler
 * for the pattern adopted here
 */
public class Handler
   extends DirContextURLStreamHandler
{
}
