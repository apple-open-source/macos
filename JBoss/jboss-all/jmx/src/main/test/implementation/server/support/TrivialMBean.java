/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.implementation.server.support;

public interface TrivialMBean
{
   void setSomething(String thing);

   String getSomething();

   void doOperation(String arg);
}
