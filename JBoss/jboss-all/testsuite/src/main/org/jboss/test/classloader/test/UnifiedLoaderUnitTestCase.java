/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.classloader.test;

import javax.management.MBeanException;
import javax.management.ObjectName;

import org.jboss.test.JBossTestCase;
import org.jboss.system.ServiceMBean;

/** Unit tests for the org.jboss.mx.loading.UnifiedLoaderRepository
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.6 $
 */
public class UnifiedLoaderUnitTestCase extends JBossTestCase
{
   public UnifiedLoaderUnitTestCase(String name)
   {
      super(name);
   }

   /** Test the UnifiedLoaderRepository for multi-threaded class loading
    */
   public void testClassLoadingMBean() throws Exception
   {
      try
      {
         deploy("concurrentloader.sar");
         ObjectName testName = new ObjectName("jboss.test:name=ConcurrentLoader");
         boolean isRegistered = getServer().isRegistered(testName);
         assertTrue(testName+" isRegistered", isRegistered);
         Integer state = (Integer) getServer().getAttribute(testName, "State");
         assertTrue("state.intValue() == ServiceMBean.STARTED",
               state.intValue() == ServiceMBean.STARTED);
      }
      finally
      {
         undeploy("concurrentloader.sar");
      } // end of try-finally
   }
   /** Test the UnifiedLoaderRepository being accessed by thread with an
    interrupted status
    */
   public void testInterruptedThreads() throws Exception
   {
      try
      {
         deploy("interrupt.sar");
         ObjectName testName = new ObjectName("jboss.test:name=InterruptTest");
         boolean isRegistered = getServer().isRegistered(testName);
         assertTrue(testName+" isRegistered", isRegistered);
         Integer state = (Integer) getServer().getAttribute(testName, "State");
         assertTrue("state.intValue() == ServiceMBean.STARTED",
               state.intValue() == ServiceMBean.STARTED);
      }
      finally
      {
         undeploy("interrupt.sar");
      } // end of try-finally
   }
   /**
    * Test the UnifiedLoaderRepository finding local and global resources
    */
   public void testResource()
      throws Exception
   {
      try
      {
         deploy("loadingresource.ear");
         ObjectName testName = new ObjectName("jboss.test:name=LoadingResource");
         boolean isRegistered = getServer().isRegistered(testName);
         assertTrue(testName+" isRegistered", isRegistered);
         Integer state = (Integer) getServer().getAttribute(testName, "State");
         assertTrue("state.intValue() == ServiceMBean.STARTED",
               state.intValue() == ServiceMBean.STARTED);
      }
      finally
      {
         undeploy("loadingresource.ear");
      } // end of try-finally
   }

   public void testUnpackedResources()
      throws Exception
   {
      deploy("unpacked/loadingresource1.ear");
      deploy("unpacked/loadingresource2.ear");

      // Assert that the version1 LoadingResource service is started
      ObjectName testName = new ObjectName("jboss.test:name=LoadingResource,version=1");
      boolean isRegistered = getServer().isRegistered(testName);
      assertTrue(testName+" isRegistered", isRegistered);
      Integer state = (Integer) getServer().getAttribute(testName, "State");
      assertTrue("state.intValue() == ServiceMBean.STARTED",
            state.intValue() == ServiceMBean.STARTED);

      // Assert that the version1 LoadingResource service is started
      testName = new ObjectName("jboss.test:name=LoadingResource,version=2");
      isRegistered = getServer().isRegistered(testName);
      assertTrue(testName+" isRegistered", isRegistered);
      state = (Integer) getServer().getAttribute(testName, "State");
      assertTrue("state.intValue() == ServiceMBean.STARTED",
            state.intValue() == ServiceMBean.STARTED);

      undeploy("unpacked/loadingresource1.ear");
      undeploy("unpacked/loadingresource2.ear");
   }
   
   public void testLoadingArrayClass() throws Exception
   {
      deploy("loadingclazz.sar");

      ObjectName testName = new ObjectName("jboss.test:name=LoadingClazz");

      boolean isRegistered = getServer().isRegistered(testName);

      assertTrue(testName + " is registered", isRegistered);

      try
      {
         try
         {
            getServer().invoke(
               testName,
               "loadClass",
               new Object[] { "[Ljava.lang.String;" },
               new String[] { String.class.getName()});
         }
         catch (MBeanException ex)
         {
            Exception tex = ex.getTargetException();
            if (tex instanceof ClassNotFoundException)
            {
               fail("java.lang.String[] not found: " + ex.getMessage());
            }
            else
            {
               throw tex;
            }
         }

         try
         {
            getServer().invoke(
               testName,
               "loadClassFromTCL",
               new Object[] { "[Ljava.lang.String;" },
               new String[] { String.class.getName()});
         }
         catch (MBeanException ex)
         {
            Exception tex = ex.getTargetException();
            if (tex instanceof ClassNotFoundException)
            {
               fail(
                  "java.lang.String[] not found from TCL: " + ex.getMessage());
            }
            else
            {
               throw tex;
            }
         }

         try
         {
            getServer().invoke(
               testName,
               "loadClass",
               new Object[] { "org.jboss.test.classloader.clazz.ClazzTest" },
               new String[] { String.class.getName()});
         }
         catch (MBeanException ex)
         {
            Exception tex = ex.getTargetException();
            if (tex instanceof ClassNotFoundException)
            {
               fail(
                  "org.jboss.test.classloader.clazz.ClazzTest not found: "
                     + ex.getMessage());
            }
            else
            {
               throw tex;
            }
         }

         try
         {
            getServer().invoke(
               testName,
               "loadClassFromTCL",
               new Object[] { "org.jboss.test.classloader.clazz.ClazzTest" },
               new String[] { String.class.getName()});
         }
         catch (MBeanException ex)
         {
            Exception tex = ex.getTargetException();
            if (tex instanceof ClassNotFoundException)
            {
               fail(
                  "org.jboss.test.classloader.clazz.ClazzTest not found from TCL: "
                     + ex.getMessage());
            }
            else
            {
               throw tex;
            }
         }

         try
         {
            getServer().invoke(
               testName,
               "loadClass",
               new Object[] { "[Lorg.jboss.test.classloader.clazz.ClazzTest;" },
               new String[] { String.class.getName()});
         }
         catch (MBeanException ex)
         {
            Exception tex = ex.getTargetException();
            if (tex instanceof ClassNotFoundException)
            {
               fail(
                  "org.jboss.test.classloader.clazz.ClazzTest[] not found: "
                     + ex.getMessage());
            }
            else
            {
               throw tex;
            }
         }

         try
         {
            getServer().invoke(
               testName,
               "loadClassFromTCL",
               new Object[] { "[Lorg.jboss.test.classloader.clazz.ClazzTest;" },
               new String[] { String.class.getName()});
         }
         catch (MBeanException ex)
         {
            Exception tex = ex.getTargetException();
            if (tex instanceof ClassNotFoundException)
            {
               fail(
                  "org.jboss.test.classloader.clazz.ClazzTest[] not found from TCL: "
                     + ex.getMessage());
            }
            else
            {
               throw tex;
            }
         }
      }
      finally
      {
         undeploy("loadingclazz.sar");
      }
   }

   protected void debug(String message)
   {
      getLog().debug(message);
   }
}
