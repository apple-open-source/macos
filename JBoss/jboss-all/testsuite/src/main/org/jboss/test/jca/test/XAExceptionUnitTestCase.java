
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jca.test;

import javax.transaction.TransactionRolledbackException;
import junit.framework.*;

import org.jboss.test.JBossTestCase;
import org.jboss.test.jca.interfaces.XAExceptionSessionHome;
import org.jboss.test.jca.interfaces.XAExceptionSession;
import org.jboss.test.jca.interfaces.XAExceptionTestSessionHome;
import org.jboss.test.jca.interfaces.XAExceptionTestSession;


/**
 * XAExceptionUnitTestCase.java
 *
 *
 * Created: Tue Sep 10 21:46:18 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class XAExceptionUnitTestCase extends JBossTestCase
{
   public XAExceptionUnitTestCase(String name)
   {
      super(name);
   }

   public static Test suite() throws Exception
   {
      Test t1 = getDeploySetup(XAExceptionUnitTestCase.class, "jcatest.jar");
      Test t2 = getDeploySetup(t1, "testadapter-ds.xml");
      return getDeploySetup(t2, "jbosstestadapter.rar");
   }

   public void testXAExceptionToTransactionRolledbackException() throws Exception
   {
      XAExceptionSessionHome xh = (XAExceptionSessionHome)getInitialContext().lookup("test/XAExceptionSessionHome");
      XAExceptionSession x = xh.create();
      try
      {
         x.testXAExceptionToTransactionRolledbackException();
      }
      catch (TransactionRolledbackException tre)
      {
         getLog().info("testXAExceptionToRollbackException passed");
         return;
      } // end of try-catch
      fail("expected TransactionRolledbackException not thrown");
   }

   public void testXAExceptionToTransactionRolledbackExceptionOnServer() throws Exception
   {
      XAExceptionTestSessionHome xth = (XAExceptionTestSessionHome)getInitialContext().lookup("test/XAExceptionTestSessionHome");
      XAExceptionTestSession xt = xth.create();
      xt.testXAExceptionToTransactionRolledbackException();
   }

   public void testXAExceptionToTransactionRolledbackLocalExceptionOnServer() throws Exception
   {
      XAExceptionTestSessionHome xth = (XAExceptionTestSessionHome)getInitialContext().lookup("test/XAExceptionTestSessionHome");
      XAExceptionTestSession xt = xth.create();
      xt.testXAExceptionToTransactionRolledbackLocalException();
   }

   public void testRMERRInOnePCToTransactionRolledbackException() throws Exception
   {
      XAExceptionSessionHome xh = (XAExceptionSessionHome)getInitialContext().lookup("test/XAExceptionSessionHome");
      XAExceptionSession x = xh.create();
      try
      {
         x.testRMERRInOnePCToTransactionRolledbackException();
      }
      catch (TransactionRolledbackException tre)
      {
         getLog().info("testXAExceptionToRollbackException passed");
         return;
      } // end of try-catch
      fail("expected TransactionRolledbackException not thrown");
   }

   public void testRMERRInOnePCToTransactionRolledbackExceptionOnServer() throws Exception
   {
      XAExceptionTestSessionHome xth = (XAExceptionTestSessionHome)getInitialContext().lookup("test/XAExceptionTestSessionHome");
      XAExceptionTestSession xt = xth.create();
      xt.testRMERRInOnePCToTransactionRolledbackException();
   }

   public void testXAExceptionToTransactionRolledbacLocalkExceptionOnServer() throws Exception
   {
      XAExceptionTestSessionHome xth = (XAExceptionTestSessionHome)getInitialContext().lookup("test/XAExceptionTestSessionHome");
      XAExceptionTestSession xt = xth.create();
      xt.testXAExceptionToTransactionRolledbacLocalkException();
   }

   public void testSimulateConnectionError() throws Exception
   {
      XAExceptionSessionHome xh = (XAExceptionSessionHome)getInitialContext().lookup("test/XAExceptionSessionHome");
      XAExceptionSession x = xh.create();
      x.simulateConnectionError();
   }

   public void testSimulateConnectionErrorWithTwoHandles() throws Exception
   {
      XAExceptionSessionHome xh = (XAExceptionSessionHome)getInitialContext().lookup("test/XAExceptionSessionHome");
      XAExceptionSession x = xh.create();
      x.simulateConnectionErrorWithTwoHandles();
   }

   public void testGetConnectionResourceError() throws Exception
   {
      XAExceptionSessionHome xh = (XAExceptionSessionHome)getInitialContext().lookup("test/XAExceptionSessionHome");
      XAExceptionSession x = xh.create();
      x.simulateError("getConnectionResource", 10);
   }

   public void testGetConnectionRuntimeError() throws Exception
   {
      XAExceptionSessionHome xh = (XAExceptionSessionHome)getInitialContext().lookup("test/XAExceptionSessionHome");
      XAExceptionSession x = xh.create();
      x.simulateError("getConnectionRuntime", 10);
   }

   public void testCreateManagedConnectionResourceError() throws Exception
   {
      XAExceptionSessionHome xh = (XAExceptionSessionHome)getInitialContext().lookup("test/XAExceptionSessionHome");
      XAExceptionSession x = xh.create();
      x.simulateFactoryError("createManagedConnectionResource", 10);
   }

   public void testCreateManagedConnectionRuntimeError() throws Exception
   {
      XAExceptionSessionHome xh = (XAExceptionSessionHome)getInitialContext().lookup("test/XAExceptionSessionHome");
      XAExceptionSession x = xh.create();
      x.simulateFactoryError("createManagedConnectionRuntime", 10);
   }

   public void testMatchManagedConnectionResourceError() throws Exception
   {
      XAExceptionSessionHome xh = (XAExceptionSessionHome)getInitialContext().lookup("test/XAExceptionSessionHome");
      XAExceptionSession x = xh.create();
      x.simulateFactoryError("matchManagedConnectionResource", 10);
   }

   public void testMatchManagedConnectionRuntimeError() throws Exception
   {
      XAExceptionSessionHome xh = (XAExceptionSessionHome)getInitialContext().lookup("test/XAExceptionSessionHome");
      XAExceptionSession x = xh.create();
      x.simulateFactoryError("matchManagedConnectionRuntime", 10);
   }

}// XAExceptionUnitTestCase
