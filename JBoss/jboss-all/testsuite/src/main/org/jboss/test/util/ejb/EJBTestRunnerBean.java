/*
 * JUnitEJB
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.util.ejb;

import java.lang.reflect.Constructor;
import java.util.Properties;
import javax.ejb.EJBException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.naming.Binding;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.naming.NamingEnumeration;
import javax.transaction.Status;
import javax.transaction.SystemException;

/**
 * Implementation of the ejb test runner.
 *
 * @see EJBTestRunner
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.3 $
 */
public class EJBTestRunnerBean implements SessionBean
{
   transient private SessionContext ctx;
   private String runnerJndiName;

   /** Run the specified test method on the given class name using a Properties
    * map built from all java:comp/env entries.
    * 
    * @param className the name of the test class
    * @param methodName the name of the test method
    * @throws RemoteTestException If any throwable is thrown during 
    * execution of the method, it is wrapped with a RemoteTestException and 
    * rethrown.
    */
   public void run(String className, String methodName)
      throws RemoteTestException
   {
      Properties props = new Properties();
      try
      {
         InitialContext ctx = new InitialContext();
         NamingEnumeration bindings = ctx.listBindings("java:comp/env");
         while( bindings.hasMore() )
         {
            Binding binding = (Binding) bindings.next();
            String name = binding.getName();
            String value = binding.getObject().toString();
            props.setProperty(name, value);
         }
      }
      catch(NamingException e)
      {
         throw new RemoteTestException(e);
      }
      run(className, methodName, props);
   }

   /** Run the specified test method on the given class name
    *  
    * @param className the name of the test class
    * @param methodName the name of the test method
    * @param props
    * @throws RemoteTestException If any throwable is thrown during 
    * execution of the method, it is wrapped with a RemoteTestException and 
    * rethrown.
    */ 
   public void run(String className, String methodName, Properties props)
      throws RemoteTestException
   {
      EJBTestCase testCase = getTestInstance(className, methodName);

      setUpEJB(testCase, props);

      RemoteTestException exception = null;
      try
      {
         runTestCase(testCase);
      }
      catch (RemoteTestException e)
      {
         exception = e;
      }
      finally
      {
         try
         {
            tearDownEJB(testCase, props);
         }
         catch (RemoteTestException e)
         {
            // favor the run exception if one was thrown
            if (exception != null)
            {
               exception = e;
            }
         }
         if (exception != null)
         {
            throw exception;
         }
      }
   }

   /**
    * Runs the setUpEJB method on the specified test case
    * @param testCase the actual test case that will be run
    * @throws RemoteTestException If any throwable is thrown during execution 
    * of the method, it is wrapped with a RemoteTestException and rethrown.
    */
   private void setUpEJB(EJBTestCase testCase, Properties props)
      throws RemoteTestException
   {
      try
      {
         ctx.getUserTransaction().begin();
         try
         {
            testCase.setUpEJB(props);
         }
         catch (Throwable e)
         {
            throw new RemoteTestException(e);
         }
         if (ctx.getUserTransaction().getStatus() == Status.STATUS_ACTIVE)
         {
            ctx.getUserTransaction().commit();
         }
      }
      catch (Throwable e)
      {
         try
         {
            ctx.getUserTransaction().rollback();
         }
         catch (SystemException unused)
         {
            // eat the exception we are exceptioning out anyway
         }
         if (e instanceof RemoteTestException)
         {
            throw (RemoteTestException) e;
         }
         throw new RemoteTestException(e);
      }
   }

   /**
    * Runs the test method on the specified test case
    * @param testCase the actual test case that will be run
    * @throws RemoteTestException If any throwable is thrown during execution 
    * of the method, it is wrapped with a RemoteTestException and rethrown.
    */
   private void runTestCase(EJBTestCase testCase) throws RemoteTestException
   {
      try
      {
         ctx.getUserTransaction().begin();
         try
         {
            testCase.runBare();
         }
         catch (Throwable e)
         {
            throw new RemoteTestException(e);
         }
         if (ctx.getUserTransaction().getStatus() == Status.STATUS_ACTIVE)
         {
            ctx.getUserTransaction().commit();
         }
      }
      catch (Throwable e)
      {
         try
         {
            ctx.getUserTransaction().rollback();
         }
         catch (SystemException unused)
         {
            // eat the exception we are exceptioning out anyway
         }
         if (e instanceof RemoteTestException)
         {
            throw (RemoteTestException) e;
         }
         throw new RemoteTestException(e);
      }
   }

   /**
    * Runs the tearDownEJB method on the specified test case
    * @param testCase the actual test case that will be run
    * @throws RemoteTestException If any throwable is thrown during execution 
    * of the method, it is wrapped with a RemoteTestException and rethrown.
    */
   private void tearDownEJB(EJBTestCase testCase, Properties props)
      throws RemoteTestException
   {

      try
      {
         ctx.getUserTransaction().begin();
         try
         {
            testCase.tearDownEJB(props);
         }
         catch (Throwable e)
         {
            throw new RemoteTestException(e);
         }
         if (ctx.getUserTransaction().getStatus() == Status.STATUS_ACTIVE)
         {
            ctx.getUserTransaction().commit();
         }
      }
      catch (Throwable e)
      {
         try
         {
            ctx.getUserTransaction().rollback();
         }
         catch (SystemException unused)
         {
            // eat the exception we are exceptioning out anyway
         }
         if (e instanceof RemoteTestException)
         {
            throw (RemoteTestException) e;
         }
         throw new RemoteTestException(e);
      }
   }

   /**
    * Gets a instance of the test class with the specified class name and
    * initialized to execute the specified method.
    *
    * @param className the name of the test class
    * @param methodName the name of the test method
    * @return a new instance of the test class with the specified class name and
    *    initialized to execute the specified method.
    */
   private EJBTestCase getTestInstance(String className, String methodName)
   {
      Class testClass = null;
      try
      {
         ClassLoader loader = Thread.currentThread().getContextClassLoader();
         testClass = loader.loadClass(className);
      }
      catch (ClassNotFoundException e)
      {
         throw new EJBException("Test class not found : " + className);
      }

      Constructor constructor = null;
      try
      {
         constructor = testClass.getConstructor(new Class[]{String.class});
      }
      catch (Exception e)
      {
         e.printStackTrace();
         throw new EJBException("Test class does not have a constructor " +
            "which has a single String argument.");
      }

      try
      {
         EJBTestCase testCase =
            (EJBTestCase) constructor.newInstance(new Object[]{methodName});
         testCase.setServerSide(true);
         return testCase;
      }
      catch (Exception e)
      {
         e.printStackTrace();
         throw new EJBException("Cannot instantiate test class: " +
            testClass.getName());
      }
   }

   public void ejbCreate()
   {
   }

   public void ejbRemove()
   {
   }

   public void ejbActivate()
   {
   }

   public void ejbPassivate()
   {
   }

   public void setSessionContext(SessionContext ctx)
   {
      this.ctx = ctx;
   }
}
