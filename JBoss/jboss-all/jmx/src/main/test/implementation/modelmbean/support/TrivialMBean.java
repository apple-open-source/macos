/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.implementation.modelmbean.support;

public interface TrivialMBean
{
   void setSomething(String thing);

   String getSomething();

   boolean doOperation(String arg);
}
