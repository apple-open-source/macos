package org.jboss.test.classloader.circularity;

import org.jboss.system.ServiceMBeanSupport;
import org.jboss.test.classloader.circularity.test.CircularityErrorTests3;
import org.jboss.test.classloader.circularity.test.CircularLoadTests;
import org.jboss.test.classloader.circularity.test.DeadlockTests3;
import org.jboss.test.classloader.circularity.test.RecursiveCCETests;

/** The MBean driver for the circularity class loading unit tests
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.2.4.9 $
 */
public class Starter extends ServiceMBeanSupport implements StarterMBean
{
   public void testClassCircularityError() throws Exception
   {
      log.info("Begin testClassCircularityError");
      try
      {
         CircularityErrorTests3 testcase0 = new CircularityErrorTests3();
         testcase0.testClassCircularityError();
      }
      finally
      {
         log.info("Begin testClassCircularityError");
      }
   }

   public void testDuplicateClass() throws Exception
   {
      log.info("Begin testDuplicateClass");
      try
      {
         CircularLoadTests testcase1 = new CircularLoadTests();
         testcase1.testDuplicateClass();
      }
      finally
      {
         log.info("Begin testDuplicateClass");
      }
   }

   public void testUCLOwner() throws Exception
   {
      log.info("Begin testUCLOwner");
      try
      {
         CircularLoadTests testcase1 = new CircularLoadTests();
         testcase1.testUCLOwner();
      }
      finally
      {
         log.info("Begin testUCLOwner");
      }
   }

   public void testLoading() throws Exception
   {
      log.info("Begin testLoading");
      try
      {
         CircularLoadTests testcase1 = new CircularLoadTests();
         testcase1.testLoading();
      }
      finally
      {
         log.info("Begin testLoading");
      }
   }

   public void testMissingSuperClass() throws Exception
   {
      log.info("Begin testMissingSuperClass");
      try
      {
         CircularLoadTests testcase1 = new CircularLoadTests();
         testcase1.testMissingSuperClass();
      }
      finally
      {
         log.info("Begin testMissingSuperClass");
      }
   }

   public void testPackageProtected() throws Exception
   {
      log.info("Begin testLinkageError");
      try
      {
         CircularLoadTests testcase1 = new CircularLoadTests();
         testcase1.testPackageProtected();
      }
      finally
      {
         log.info("Begin testDeadlockCase1");
      }
   }

   public void testLinkageError() throws Exception
   {
      log.info("Begin testLinkageError");
      try
      {
         CircularLoadTests testcase1 = new CircularLoadTests();
         testcase1.testLinkageError();
      }
      finally
      {
         log.info("Begin testLinkageError");
      }
   }

   public void testDeadlockCase1() throws Exception
   {
      log.info("Begin testDeadlockCase1");
      try
      {
         DeadlockTests3 testcase1 = new DeadlockTests3();
         testcase1.testDeadlockCase1();
      }
      finally
      {
         log.info("Begin testDeadlockCase1");
      }
   }

   public void testRecursiveLoadMT() throws Exception
   {
      log.info("Begin testRecursiveLoadMT");
      try
      {
         RecursiveCCETests testcase1 = new RecursiveCCETests();
         testcase1.testRecursiveLoadMT();
      }
      catch(Exception e)
      {
         log.error("testRecursiveLoadMT", e);
         throw e;
      }
      finally
      {
         log.info("Begin testRecursiveLoadMT");
      }
   }
}
