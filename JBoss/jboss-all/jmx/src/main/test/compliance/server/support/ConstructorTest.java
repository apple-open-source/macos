/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.server.support;

/**
 * Support class which throws a checked exception from its constructor.
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.1 $
 *   
 */
public class ConstructorTest
{
   // Constructors --------------------------------------------------
   public ConstructorTest() throws MyScreamingException
   {
      throw new MyScreamingException();
   }
   
}
      



