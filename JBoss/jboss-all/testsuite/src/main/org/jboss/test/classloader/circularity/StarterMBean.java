package org.jboss.test.classloader.circularity;

import org.jboss.system.ServiceMBean;

/** The MBean driver for the circularity class loading unit tests
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.5 $
 */
public interface StarterMBean extends ServiceMBean
{
   public void testClassCircularityError() throws Exception;
   public void testDuplicateClass() throws Exception;
   public void testUCLOwner() throws Exception;
   public void testLoading() throws Exception;
   public void testMissingSuperClass() throws Exception;
   public void testPackageProtected() throws Exception;
   public void testLinkageError() throws Exception;
   public void testDeadlockCase1() throws Exception;
   public void testRecursiveLoadMT() throws Exception;
}
