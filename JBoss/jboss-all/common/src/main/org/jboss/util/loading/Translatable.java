/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.util.loading;

import java.net.URL;

public interface Translatable
{
   public Class loadClass(String name) throws ClassNotFoundException;
   public Class loadClass(String name, byte[] byteCode) throws ClassFormatError;
   public URL getResourceLocally(String name);
}
