package org.jboss.test.jbossmx.implementation.server.support;

import java.io.IOException;
import java.io.*;
import java.rmi.MarshalledObject;

import org.apache.log4j.Category;

/** The ContextCL standard MBean implementation.
 *
 * @author  Scott.Stark@jboss.org
 * @version $Revision: 1.2 $
 */
public class ContextCL implements ContextCLMBean
{
   private static Category log = Category.getInstance(ContextCL.class);
   private TestData data0;

   /** The TestData.class ClassLoader will be the ClassLoader of the ContextCL
    *mbean because we have a static reference to the TestData class. This
    *causes the VM to call the ClassLoader.loadClassInternal method.
    */
   public ContextCL() throws IOException
   {
      ClassLoader cl = Thread.currentThread().getContextClassLoader();
      log.info("ContextCL ClassLoader: "+getClass().getClassLoader());
      log.info("ctor Context ClassLoader: "+cl);
      data0 = new TestData();
      log.info("TestData.class ProtectionDomain: "+TestData.class.getProtectionDomain());
   }

   /** An operation that load the TestData class using the current thread
    *context class loader (TCL) and the Class.forName(String, boolean, ClassLoader)
    *operation to validate that the class loader used to load TestData in
    *the ctor is compatible with the operation TCL.
    */
   public void useTestData() throws Exception
   {
      ClassLoader cl = Thread.currentThread().getContextClassLoader();
      log.info("useTestData ClassLoader: "+cl);
      Class c0 = data0.getClass();
      log.info("TestData #0 ProtectionDomain: "+c0.getProtectionDomain());
      Class c1 = Class.forName("org.jboss.test.jbossmx.implementation.server.support.TestData",
         false, cl);
      log.info("TestData #1 ProtectionDomain: "+c1.getProtectionDomain());
      if( c1.isInstance(data0) == false )
      {
         log.error("Assertion failed: data0 is NOT compatible with c1");
         throw new IllegalStateException("data0 is NOT compatible with c1");
      }
   }
}
