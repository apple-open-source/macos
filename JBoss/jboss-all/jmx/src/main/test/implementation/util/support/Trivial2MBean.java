/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.implementation.util.support;

/**
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.1 $
 */
public interface Trivial2MBean
{
   void setSomething(String thing);

   String getSomething();

   void doOperation();
   
   boolean isOperationInvoked();
   
   void reset();
}
