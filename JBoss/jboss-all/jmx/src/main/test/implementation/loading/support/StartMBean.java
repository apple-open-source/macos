/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.implementation.loading.support;

/**
 * Start MBean invokes Target MBean with an arg AClass that
 * both MBeans have loaded using different MLet loaders.
 *
 * @see <related>
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.1 $
 *   
 */
public interface StartMBean
{

   public void startOp(String agentID) throws Exception;
   
}
      



