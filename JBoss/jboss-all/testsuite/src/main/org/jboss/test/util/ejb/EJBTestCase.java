/*
 * JUnitEJB
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.util.ejb;


import java.util.Properties;
import javax.naming.InitialContext;
import javax.rmi.PortableRemoteObject;

import junit.framework.AssertionFailedError;
import junit.framework.TestCase;
import junit.framework.TestResult;

/**
 * An ejb test case is an extension to test case where the test is executed
 * in the ejb server's virtual machine.
 *
 * Two new methods setUpEJB and tearDownEJB have been added. These methods 
 * work just like setUp and tearDown except they run in a sepperate transaction.
 * The execution order is as follows:
 * <pre>
 * 	1. setUpEJB (TX 1)
 * 	2. run (TX 2)
 * 		2.1. runBare
 * 			2.1.1 setUp
 * 			2.1.2 <your test method>
 * 			2.1.3 tearDown
 * 	3. ejbTearDown (TX 2)
 * </pre>
 *
 * For an ejb test case to run successfully, the following must be setup:
 * <pre>
 * 	1. The ejb test case class must be availabe to the client vm.
 * 	2. The ejb test case class must be availabe to the EJBTestRunner bean
 * 			on the server.
 * 	3. The EJBTestRunnerHome must be bound to "ejb/EJBTestRunner" in the
 * 			jndi context obtained from new InitialContext();
 * 	4. The EJBTestRunner bean must be configured as specified in the 
 * 			EJBTestRunner javadoc.
 * </pre>
 *
 * @see EJBTestRunner
 * @see junit.framework.TestCase
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.5 $
 */
public class EJBTestCase extends TestCase
{
   private boolean serverSide = false;
   protected Properties props;

   /**
    * Constructs a test case that will run the method with the specified name.
    * @param methodName the name of the method that will executed when this 
    * 		test is run
    */
   public EJBTestCase(String methodName)
   {
      super(methodName);
   }

   /**
    * Sets the flag that is used to determine if the class
    * is running on the server side.
    * @param serverSide boolean flag that this class uses to determine
    * 		if it's running on the server side.
    */
   public void setServerSide(boolean serverSide)
   {
      this.serverSide = serverSide;
   }

   /**
    * Is this class running on the server side?
    * @return true if this class is running on the server side	 
    */
   public boolean isServerSide()
   {
      return serverSide;
   }

   /** Allow EJBTestCase subclasses to override the EJBRunnerHome JNDI name
    * @return The JNDI name of the EJBRunnerHome home interface binding. The
    * default is "ejb/EJBTestRunner"
    */ 
   public String getEJBRunnerJndiName()
   {
      return "ejb/EJBTestRunner";
   }

   /**
    * @return the properties associated with the test case
    */ 
   public Properties getProps()
   {
      return props;
   }
   /**
    * @param props the properties associated with the test case
    */ 
   public void setProps(Properties props)
   {
      this.props = props;
   }

   public void run(TestResult result)
   {
      ClassLoader oldClassLoader = null;
      try
      {
         // If we are on the server side, set the thread context class loader
         // to the class loader that loaded this class. This fixes problems
         // with the current implementation of the JUnit gui test runners class
         // reloading logic. The gui relods the test classes with each run but 
         // does not set the context class loader so calls to Class.forName load
         // the class in the wrong class loader.
         if (!isServerSide())
         {
            oldClassLoader = Thread.currentThread().getContextClassLoader();
            Thread.currentThread().setContextClassLoader(
               getClass().getClassLoader());
         }

         super.run(result);
      }
      finally
      {
         // be a good citizen, reset the context loader
         if (oldClassLoader != null)
         {
            Thread.currentThread().setContextClassLoader(oldClassLoader);
         }
      }
   }

   public void runBare() throws Throwable
   {
      if (!isServerSide())
      {
         // We're not on the server side yet, invoke the test on the serverside.
         EJBTestRunner testRunner = null;
         try
         {
            testRunner = getEJBTestRunner();
            if( props != null )
               testRunner.run(getClass().getName(), getName(), props);
            else
               testRunner.run(getClass().getName(), getName());
         }
         catch (RemoteTestException e)
         {
            // if the remote test exception is from an assertion error
            // rethrow it with a sub class of AssertionFailedError so it is 
            // picked up as a failure and not an error.
            // The server has to throw sub classes of Error because that is the
            // allowable scope of application exceptions. So 
            // AssertionFailedError which is an instance of error has to be
            // wrapped in an exception.
            Throwable remote = e.getRemoteThrowable();
            if (remote instanceof AssertionFailedError)
            {
               throw new RemoteAssertionFailedError(
                  (AssertionFailedError) remote, e.getRemoteStackTrace());
            }
            throw e;
         }
         finally
         {
            // be a good citizen, drop my ref to the session bean.
            if (testRunner != null)
            {
               testRunner.remove();
            }
         }
      }
      else
      {
         // We're on the server side so, invoke the test the usual way.
         super.runBare();
      }
   }

   /** Sets up the ejb test case. This method is called before
    * each test is executed and is run in a private transaction.
    * @param props the properties passed in from the client
    * @throws Exception if a problem occures
    */
   public void setUpEJB(Properties props) throws Exception
   {
      this.props = props;
   }

   /** Tears down the ejb test case. This method is called after
    * each test is executed and is run in a private transaction.
    * @param props the properties passed in from the client
    * @throws Exception if a problem occures
    */
   public void tearDownEJB(Properties props) throws Exception
   {
   }

   /**
    * Looks up the ejb test runner home in JNDI (at "ejb/EJBTestRunner")
    * and creates a new runner.
    * @throws Exception if any problem happens
    */
   private EJBTestRunner getEJBTestRunner() throws Exception
   {
      InitialContext jndiContext = new InitialContext();

      // Get a reference from this to the Bean's Home interface
      String name = getEJBRunnerJndiName();
      Object ref = jndiContext.lookup(name);
      EJBTestRunnerHome runnerHome = (EJBTestRunnerHome)
         PortableRemoteObject.narrow(ref, EJBTestRunnerHome.class);

      // create the test runner
      return runnerHome.create();
   }
}
