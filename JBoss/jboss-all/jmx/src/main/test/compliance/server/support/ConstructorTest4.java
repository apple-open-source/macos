/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.server.support;

/**
 * Support class which fails with an unchecked exception in static initializer.
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.1 $
 *   
 */
public class ConstructorTest4
{

   static
   {
      Object o = null;
      o.toString();        // causes NPE!      
   }
   
}
      



