/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.util.loading;

public interface Translator
{
   public byte[] translate(String classname, ClassLoader cl) throws Exception;

   public void unregisterClassLoader(ClassLoader cl);
}
