/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.implementation.loading;

import junit.framework.TestCase;
import junit.framework.AssertionFailedError;

import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.ObjectName;
import javax.management.Attribute;
import javax.management.ServiceNotFoundException;
import javax.management.MBeanException;
import javax.management.ReflectionException;
import javax.management.RuntimeErrorException;
import javax.management.RuntimeMBeanException;

import javax.management.loading.MLet;
import javax.management.loading.MLetMBean;
import javax.management.loading.DefaultLoaderRepository;

import org.jboss.mx.util.AgentID;
import org.jboss.mx.server.ServerConstants;

public class LoaderRepositoryTEST extends TestCase implements ServerConstants
{
   public LoaderRepositoryTEST(String s)
   {
      super(s);
   }

   public void testClassConflictBetweenMLets() throws Exception
   {
      // NOTE:
      // the urls used here are relative to the location of the build.xml
 
      // make sure the classes are loaded from mlet, not system cl
      String[] classes = { "test.implementation.loading.support.Start",
                           "test.implementation.loading.support.StartMBean",
                           "test.implementation.loading.support.Target",
                           "test.implementation.loading.support.TargetMBean",
                           "test.implementation.loading.support.AClass"
      };
            
      for (int i = 0; i < classes.length; ++i)
      {
         try
         {
            DefaultLoaderRepository.loadClass(classes[i]);

            fail("class " + classes[i] + " was already found in CL repository.");
         }
         catch (ClassNotFoundException e)
         {
            // expected
         }
      }
      
      try
      {
         MBeanServer server = MBeanServerFactory.createMBeanServer();
         MLet mlet1 = new MLet();
         MLet mlet2 = new MLet();
         ObjectName m1Name = new ObjectName(":name=mlet1");
         ObjectName m2Name = new ObjectName(":name=mlet2");
         
         server.registerMBean(mlet1, m1Name);
         server.registerMBean(mlet2, m2Name);
         
         server.invoke(m1Name, "getMBeansFromURL",
         new Object[] { "file:./output/etc/test/implementation/loading/CCTest1.mlet" },
         new String[] { String.class.getName() }
         );
         
         server.invoke(m2Name, "getMBeansFromURL",
         new Object[] { "file:./output/etc/test/implementation/loading/CCTest2.mlet" },
         new String[] { String.class.getName() }
         );
         
         server.invoke(new ObjectName(":name=Start"), "startOp", 
         new Object[] { AgentID.get(server) },
         new String[] { String.class.getName() }
         );
         
         //fail("Expected to fail due to two different mlet loaders having a class mismatch.");
      }
      catch (MBeanException e)
      {
         if (e.getTargetException() instanceof ReflectionException)
         {
            // expected, argument type mismatch error since the arg of type AClass is
            // loaded by diff mlet loader than the target MBean with AClass in its sign.
            if (System.getProperty(LOADER_REPOSITORY_CLASS_PROPERTY).equals(UNIFIED_LOADER_REPOSITORY_CLASS))
               throw e;
         }
         else throw e;
      }
   }

}
