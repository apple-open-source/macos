/* 
 * ====================================================================
 * This is Open Source Software, distributed
 * under the Apache Software License, Version 1.1
 * 
 * 
 *  This software  consists of voluntary contributions made  by many individuals
 *  on  behalf of the Apache Software  Foundation and was  originally created by
 *  Ivelin Ivanov <ivelin@apache.org>. For more  information on the Apache
 *  Software Foundation, please see <http://www.apache.org/>.
 */

package org.jboss.test.ha.singleton.test;

import javax.management.ObjectName;
import junit.framework.TestCase;

import org.jboss.test.ha.singleton.HASingletonControllerTester;

/**
 * 
 * @author  Ivelin Ivanov <ivelin@apache.org>
 * @version $Revision: 1.1.2.3 $
 */
public class HASingletonControllerUnitTestCase extends TestCase
{

   private HASingletonControllerTester singletonControllerTester = null;

   public HASingletonControllerUnitTestCase(String name)
   {
      super(name);
   }

   public void setUp()
   {
      singletonControllerTester = new HASingletonControllerTester();
   }


   public void tearDown()
   {
      singletonControllerTester = null;
   }

   public void testSetValidTargetName() throws Exception
   {
      ObjectName someSingletonService = new ObjectName("jboss:service=HASingletonMBeanExample");
      singletonControllerTester.setTargetName(someSingletonService);

      assertEquals("setTargetName() failed", singletonControllerTester.getTargetName(), someSingletonService);
   }

   public void testSetTargetStartMethod()
   {
      String someMethod = "startTheSingleton";
      singletonControllerTester.setTargetStartMethod(someMethod);

      assertEquals("setTargetStartMethod() failed", singletonControllerTester.getTargetStartMethod(), someMethod);
   }

   public void testSetTargetStartMethodArgument()
   {
       String someArgument = "aStartValue";
       singletonControllerTester.setTargetStartMethodArgument(someArgument);
  
       assertEquals("setTargetStartMethodArgument() failed", singletonControllerTester.getTargetStartMethodArgument(), someArgument);
   }

  public void testSetTargetStopMethodArgument()
  {
      String someArgument = "aSopValue";
      singletonControllerTester.setTargetStopMethodArgument(someArgument);
  
      assertEquals("setTargetStopMethodArgument() failed", singletonControllerTester.getTargetStopMethodArgument(), someArgument);
  }

   public void testSetNullOrBlankStartTargetName()
   {
      String someMethod = "";
      singletonControllerTester.setTargetStartMethod(someMethod);

      assertEquals("setTargetStartMethod() failed to set default value", singletonControllerTester.getTargetStartMethod(), "startSingleton");

      someMethod = null;
      singletonControllerTester.setTargetStartMethod(someMethod);

      assertEquals("setTargetStartMethod() failed to set default value", singletonControllerTester.getTargetStartMethod(), "startSingleton");
   }


   public void testSetTargetStopMethod()
   {
      String someMethod = "stopTheSingleton";
      singletonControllerTester.setTargetStopMethod(someMethod);

      assertEquals("setTargetStartMethod() failed", singletonControllerTester.getTargetStopMethod(), someMethod);
   }


   public void testSetNullOrBlankStopTargetName()
   {
      String someMethod = "";
      singletonControllerTester.setTargetStopMethod(someMethod);

      assertEquals("setTargetStartMethod() failed to set default value", singletonControllerTester.getTargetStopMethod(), "stopSingleton");

      someMethod = null;
      singletonControllerTester.setTargetStopMethod(someMethod);

      assertEquals("setTargetStartMethod() failed to set default value", singletonControllerTester.getTargetStopMethod(), "stopSingleton");
   }


   public void testStartSingleton() throws Exception
   {
      ObjectName serviceName = new ObjectName("jboss:service=HASingletonMBeanExample");
      singletonControllerTester.setTargetName(serviceName);
      singletonControllerTester.setTargetStartMethod("startTheSingleton");

      singletonControllerTester.startSingleton();

      assertEquals("method not invoked as expected",
         singletonControllerTester.__invokationStack__.pop(), "invokeMBeanMethod:jboss:service=HASingletonMBeanExample.startTheSingleton");
   }

   public void testStopSingleton() throws Exception
   {
      ObjectName serviceName = new ObjectName("jboss:service=HASingletonMBeanExample");
      singletonControllerTester.setTargetName(serviceName);
      singletonControllerTester.setTargetStopMethod("stopTheSingleton");

      singletonControllerTester.stopSingleton();

      assertEquals("method not invoked as expected",
         singletonControllerTester.__invokationStack__.pop(), "invokeMBeanMethod:jboss:service=HASingletonMBeanExample.stopTheSingleton");
   }

}
