/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.implementation.registry;

import junit.framework.TestCase;

import test.implementation.registry.support.Trivial;

import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.ObjectName;

import java.util.Date;
import java.util.Map;
import java.util.HashMap;

import org.jboss.mx.server.ServerConstants;

/**
 * Tests the value map processing in the managed mbean registry
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class ValuesTestCase
   extends TestCase
   implements ServerConstants
{
   // Attributes ----------------------------------------------------------------

   // Constructor ---------------------------------------------------------------

   /**
    * Construct the test
    */
   public ValuesTestCase(String s)
   {
      super(s);
   }

   // Tests ---------------------------------------------------------------------

   /**
    * Test classloader
    */
   public void testClassLoader()
      throws Exception
   {
      // Get the previous registry value
      String previous = System.getProperty(MBEAN_REGISTRY_CLASS_PROPERTY);
      if (previous == null)
         previous = DEFAULT_MBEAN_REGISTRY_CLASS;
      //System.setProperty(MBEAN_REGISTRY_CLASS_PROPERTY, 
      //                   "org.jboss.mx.server.registry.ManagedMBeanRegistry");
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         HashMap valuesMap = new HashMap();
         ClassLoader cl = Thread.currentThread().getContextClassLoader();
         valuesMap.put(CLASSLOADER, Thread.currentThread().getContextClassLoader());
         ObjectName mbean = new ObjectName("test:test=test");
         server.invoke(new ObjectName(MBEAN_REGISTRY), "registerMBean",
            new Object[]
            {
               new Trivial(),
               mbean,
               valuesMap
            },
            new String[]
            {
               Object.class.getName(),
               ObjectName.class.getName(),
               Map.class.getName()
            }
         );
         Object result = server.invoke(new ObjectName(MBEAN_REGISTRY), "getValue",
            new Object[]
            {
               mbean,
               CLASSLOADER
            },
            new String[]
            {
               ObjectName.class.getName(),
               String.class.getName()
            }
         );
         assertEquals(cl, result);
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
         System.setProperty(MBEAN_REGISTRY_CLASS_PROPERTY, previous);
      }
   }

   /**
    * Test value
    */
   public void testValue()
      throws Exception
   {
      // Get the previous registry value
      String previous = System.getProperty(MBEAN_REGISTRY_CLASS_PROPERTY);
      if (previous == null)
         previous = DEFAULT_MBEAN_REGISTRY_CLASS;
      //System.setProperty(MBEAN_REGISTRY_CLASS_PROPERTY, 
      //                   "org.jboss.mx.server.registry.ManagedMBeanRegistry");
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         HashMap valuesMap = new HashMap();
         Date date = new Date(System.currentTimeMillis());
         valuesMap.put("date", date);
         ObjectName mbean = new ObjectName("test:test=test");
         server.invoke(new ObjectName(MBEAN_REGISTRY), "registerMBean",
            new Object[]
            {
               new Trivial(),
               mbean,
               valuesMap
            },
            new String[]
            {
               Object.class.getName(),
               ObjectName.class.getName(),
               Map.class.getName()
            }
         );
         Object result = server.invoke(new ObjectName(MBEAN_REGISTRY), "getValue",
            new Object[]
            {
               mbean,
               "date"
            },
            new String[]
            {
               ObjectName.class.getName(),
               String.class.getName()
            }
         );
         assertEquals(date, result);
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
         System.setProperty(MBEAN_REGISTRY_CLASS_PROPERTY, previous);
      }
   }

   /**
    * Test value registered
    */
   public void testValueRegistered()
      throws Exception
   {
      // Get the previous registry value
      String previous = System.getProperty(MBEAN_REGISTRY_CLASS_PROPERTY);
      if (previous == null)
         previous = DEFAULT_MBEAN_REGISTRY_CLASS;
      //System.setProperty(MBEAN_REGISTRY_CLASS_PROPERTY, 
      //                   "org.jboss.mx.server.registry.ManagedMBeanRegistry");
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         HashMap valuesMap = new HashMap();
         Date date = new Date(System.currentTimeMillis());
         valuesMap.put("date", date);
         ObjectName mbean = new ObjectName("test:test=test");
         server.invoke(new ObjectName(MBEAN_REGISTRY), "registerMBean",
            new Object[]
            {
               new Trivial(),
               mbean,
               valuesMap
            },
            new String[]
            {
               Object.class.getName(),
               ObjectName.class.getName(),
               Map.class.getName()
            }
         );
         Object result = server.invoke(new ObjectName(MBEAN_REGISTRY), "getValue",
            new Object[]
            {
               mbean,
               "date"
            },
            new String[]
            {
               ObjectName.class.getName(),
               String.class.getName()
            }
         );
         assertEquals(date, result);

         // Now remove it, reregister it and make sure it returns the new value
         server.unregisterMBean(mbean);

         Thread.currentThread().sleep(2);
         date = new Date(System.currentTimeMillis());
         HashMap valueMap2 = new HashMap();
         valueMap2.put("date", date);
         server.invoke(new ObjectName(MBEAN_REGISTRY), "registerMBean",
            new Object[]
            {
               new Trivial(),
               mbean,
               valueMap2
            },
            new String[]
            {
               Object.class.getName(),
               ObjectName.class.getName(),
               Map.class.getName()
            }
         );
         result = server.invoke(new ObjectName(MBEAN_REGISTRY), "getValue",
            new Object[]
            {
               mbean,
               "date"
            },
            new String[]
            {
               ObjectName.class.getName(),
               String.class.getName()
            }
         );
         assertEquals(date, result);
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
         System.setProperty(MBEAN_REGISTRY_CLASS_PROPERTY, previous);
      }
   }
}
