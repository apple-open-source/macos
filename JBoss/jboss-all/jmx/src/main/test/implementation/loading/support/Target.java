/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.implementation.loading.support;

/**
 * Target attempts to place the arg to instance field.
 *
 * @see <related>
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.1 $
 *   
 */
public class Target
   implements TargetMBean
{

   AClass a;
   
   public void executeTarget(AClass a)
   {
      this.a = a;
   }
   
}
      



