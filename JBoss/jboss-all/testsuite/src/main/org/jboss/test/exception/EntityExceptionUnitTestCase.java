package org.jboss.test.exception;

import java.rmi.RemoteException;
import java.util.Collection;
import java.util.HashSet;
import java.util.Set;
import java.util.Iterator;
import javax.naming.InitialContext;
import javax.ejb.EJBException;
import javax.ejb.EJBLocalObject;
import javax.ejb.FinderException;
import javax.ejb.TransactionRolledbackLocalException;
import javax.transaction.TransactionRolledbackException;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;
import org.apache.log4j.Category;
import net.sourceforge.junitejb.EJBTestCase;

import org.jboss.test.JBossTestCase;

public class EntityExceptionUnitTestCase extends EJBTestCase {
   public static Test suite() throws Exception {
		return JBossTestCase.getDeploySetup(
            EntityExceptionUnitTestCase.class, 
            "exception.jar");
   }   

   public EntityExceptionUnitTestCase(String name) {
      super(name);
   }

   private Category log = Category.getInstance(getClass());
   private EntityExceptionTesterHome exceptionTesterHome;
   private EntityExceptionTesterLocalHome exceptionTesterLocalHome;

   /**
    * Looks up all of the home interfaces and creates the initial data. 
    * Looking up objects in JNDI is expensive, so it should be done once 
    * and cached.
    * @throws Exception if a problem occures while finding the home interfaces,
    * or if an problem occures while createing the initial data
    */
   public void setUp() throws Exception {
      InitialContext jndi = new InitialContext();

      exceptionTesterHome = (EntityExceptionTesterHome) 
            jndi.lookup("exception/EntityExceptionTester"); 

      exceptionTesterLocalHome = (EntityExceptionTesterLocalHome) 
            jndi.lookup("exception/EntityExceptionTesterLocal"); 
   }

   public void testApplicationExceptionInTx_remote() throws Exception {
      EntityExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create(getName());

         exceptionTester.applicationExceptionInTx();

         fail("Expected application exception to be thrown");

      } catch(ApplicationException e) {
         // good this was expected
      } catch(Exception e) {
         fail("Expected ApplicationException but got " + e);
      }
   }

   public void testNotDiscardedApplicationExceptionInTx_remote() throws Exception
   {
      exceptionTesterHome.findByPrimaryKey("testApplicationExceptionInTx_remote"); 
   }

   public void testApplicationExceptionInTxMarkRollback_remote() throws Exception {
      EntityExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create(getName());

         exceptionTester.applicationExceptionInTxMarkRollback();

         fail("Expected application exception to be thrown");

      } catch(ApplicationException e) {
         // good this was expected
      } catch(Exception e) {
         fail("Expected ApplicationException but got " + e);
      }
   }

   public void testNotDiscardedApplicationExceptionInTxMarkRollback_remote() throws Exception
   {
      exceptionTesterHome.findByPrimaryKey("testApplicationExceptionInTxMarkRollback_remote"); 
   }

   public void testApplicationErrorInTx_remote() throws Exception {
      EntityExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create(getName());

         exceptionTester.applicationErrorInTx();

         fail("Expected transaction rolled back exception to be thrown");

      } catch(TransactionRolledbackException e) {
         // good this was expected
         assertNotNull(
               "TransactionRolledbackException.detail should not be null",
               e.detail);

         assertEquals(
               "TransactionRolledbackException.detail should " +
                     "be a ApplicationError",
               ApplicationError.class,
               e.detail.getClass());

      } catch(Exception e) {
         fail("Expected TransactionRolledbackException but got " + e);
      }
   }

   public void testDiscardedApplicationErrorInTx_remote() throws Exception
   {
      boolean caught = false;
      try
      {
        exceptionTesterHome.findByPrimaryKey("testApplicationErrorInTx_remote"); 
      }
      catch (FinderException expected)
      {
         caught = true;
      }
      assertTrue("Instance not discarded " + getName(), caught);
   }

   public void testEJBExceptionInTx_remote() throws Exception {
      EntityExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create(getName());

         exceptionTester.ejbExceptionInTx();

         fail("Expected transaction rolled back exception to be thrown");

      } catch(TransactionRolledbackException e) {
         // good this was expected
         assertNotNull(
               "TransactionRolledbackException.detail should not be null",
               e.detail);
         assertEquals(
               "TransactionRolledbackException.detail should " +
                     "be an EJBException",
               EJBException.class,
               e.detail.getClass());

      } catch(Exception e) {
         fail("Expected TransactionRolledbackException but got " + e);
      }
   }

   public void testDiscardedEJBExceptionInTx_remote() throws Exception
   {
      boolean caught = false;
      try
      {
        exceptionTesterHome.findByPrimaryKey("testEJBExceptionInTx_remote"); 
      }
      catch (FinderException expected)
      {
         caught = true;
      }
      assertTrue("Instance not discarded " + getName(), caught);
   }

   public void testRuntimeExceptionInTx_remote() throws Exception {
      EntityExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create(getName());

         exceptionTester.runtimeExceptionInTx();

         fail("Expected transaction rolled back exception to be thrown");

      } catch(TransactionRolledbackException e) {
         // good this was expected
         assertNotNull(
               "TransactionRolledbackException.detail should not be null",
               e.detail);

         assertEquals(
               "TransactionRolledbackException.detail should " +
                     "be a RuntimeException",
               RuntimeException.class,
               e.detail.getClass());

      } catch(Exception e) {
         fail("Expected TransactionRolledbackException but got " + e);
      }
   }

   public void testDiscardedRuntimeExceptionInTx_remote() throws Exception
   {
      boolean caught = false;
      try
      {
        exceptionTesterHome.findByPrimaryKey("testRuntimeExceptionInTx_remote"); 
      }
      catch (FinderException expected)
      {
         caught = true;
      }
      assertTrue("Instance not discarded " + getName(), caught);
   }

   public void testRemoteExceptionInTx_remote() throws Exception {
      EntityExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create(getName());

         exceptionTester.remoteExceptionInTx();

         fail("Expected transaction rolled back exception to be thrown");

      } catch(TransactionRolledbackException e) {
         // good this was expected
         assertNotNull(
               "TransactionRolledbackException.detail should not be null",
               e.detail);
         assertEquals(
               "TransactionRolledbackException.detail should " +
                     "be a RemoteException",
               RemoteException.class,
               e.detail.getClass());

      } catch(Exception e) {
         fail("Expected TransactionRolledbackException but got " + e);
      }
   }

   public void testDiscardedRemoteExceptionInTx_remote() throws Exception
   {
      boolean caught = false;
      try
      {
        exceptionTesterHome.findByPrimaryKey("testRemoteExceptionInTx_remote"); 
      }
      catch (FinderException expected)
      {
         caught = true;
      }
      assertTrue("Instance not discarded " + getName(), caught);
   }

   public void testApplicationExceptionNewTx_remote() throws Exception {
      EntityExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create(getName());

         exceptionTester.applicationExceptionNewTx();

         fail("Expected application exception to be thrown");

      } catch(ApplicationException e) {
         // good this was expected
      } catch(Exception e) {
         fail("Expected ApplicationException but got " + e);
      }
   }

   public void testNotDiscardedApplicationExceptionNewTx_remote() throws Exception
   {
      exceptionTesterHome.findByPrimaryKey("testApplicationExceptionNewTx_remote"); 
   }

   public void testApplicationExceptionNewTxMarkRollback_remote() throws Exception {
      EntityExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create(getName());

         exceptionTester.applicationExceptionNewTxMarkRollback();

         fail("Expected application exception to be thrown");

      } catch(ApplicationException e) {
         // good this was expected
      } catch(Exception e) {
         fail("Expected ApplicationException but got " + e);
      }
   }

   public void testNotDiscardedApplicationExceptionNewTxMarkRollback_remote() throws Exception
   {
      exceptionTesterHome.findByPrimaryKey("testApplicationExceptionNewTxMarkRollback_remote"); 
   }

   public void testApplicationErrorNewTx_remote() throws Exception {
      EntityExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create(getName());

         exceptionTester.applicationErrorNewTx();

         fail("Expected RemoteException to be thrown");

      } catch(RemoteException e) {
         // good this was expected
         assertNotNull(
               "RemoteException.detail should not be null",
               e.detail);

         assertEquals(
               "RemoteException.detail should be a ApplicationError",
               ApplicationError.class,
               e.detail.getClass());
      } catch(Exception e) {
         fail("Expected RemoteException but got " + e);
      }
   }

   public void testDiscardedApplicationErrorNewTx_remote() throws Exception
   {
      boolean caught = false;
      try
      {
        exceptionTesterHome.findByPrimaryKey("testApplicationErrorNewTx_remote"); 
      }
      catch (FinderException expected)
      {
         caught = true;
      }
      assertTrue("Instance not discarded " + getName(), caught);
   }

   public void testEJBExceptionNewTx_remote() throws Exception {
      EntityExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create(getName());

         exceptionTester.ejbExceptionNewTx();

         fail("Expected RemoteException to be thrown");

      } catch(RemoteException e) {
         // good this was expected
         assertNotNull(
               "RemoteException.detail should not be null",
               e.detail);

         assertEquals(
               "RemoteException.detail should be a EJBException",
               EJBException.class,
               e.detail.getClass());
      } catch(Exception e) {
         fail("Expected RemoteException but got " + e);
      }
   }

   public void testDiscardedEJBExceptionNewTx_remote() throws Exception
   {
      boolean caught = false;
      try
      {
        exceptionTesterHome.findByPrimaryKey("testEJBExceptionNewTx_remote"); 
      }
      catch (FinderException expected)
      {
         caught = true;
      }
      assertTrue("Instance not discarded " + getName(), caught);
   }

   public void testRuntimeExceptionNewTx_remote() throws Exception {
      EntityExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create(getName());

         exceptionTester.runtimeExceptionNewTx();

         fail("Expected RemoteException to be thrown");

      } catch(RemoteException e) {
         // good this was expected
         assertNotNull(
               "RemoteException.detail should not be null",
               e.detail);

         assertEquals(
               "RemoteException.detail should be a RuntimeException",
               RuntimeException.class,
               e.detail.getClass());

      } catch(Exception e) {
         fail("Expected RemoteException but got " + e);
      }
   }

   public void testDiscardedRuntimeExceptionNewTx_remote() throws Exception
   {
      boolean caught = false;
      try
      {
        exceptionTesterHome.findByPrimaryKey("testRuntimeExceptionNewTx_remote"); 
      }
      catch (FinderException expected)
      {
         caught = true;
      }
      assertTrue("Instance not discarded " + getName(), caught);
   }

   public void testRemoteExceptionNewTx_remote() throws Exception {
      EntityExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create(getName());

         exceptionTester.remoteExceptionNewTx();

         fail("Expected RemoteException to be thrown");

      } catch(RemoteException e) {
         // good this was expected
      } catch(Exception e) {
         fail("Expected RemoteException but got " + e);
      }
   }

   public void testDiscardedRemoteExceptionNewTx_remote() throws Exception
   {
      boolean caught = false;
      try
      {
        exceptionTesterHome.findByPrimaryKey("testRemoteExceptionNewTx_remote"); 
      }
      catch (FinderException expected)
      {
         caught = true;
      }
      assertTrue("Instance not discarded " + getName(), caught);
   }

   public void testApplicationExceptionNoTx_remote() throws Exception {
      EntityExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create(getName());

         exceptionTester.applicationExceptionNoTx();

         fail("Expected application exception to be thrown");

      } catch(ApplicationException e) {
         // good this was expected
      } catch(Exception e) {
         fail("Expected ApplicationException but got " + e);
      }
   }

   public void testNotDiscardedApplicationExceptionNoTx_remote() throws Exception
   {
      exceptionTesterHome.findByPrimaryKey("testApplicationExceptionNoTx_remote"); 
   }

   public void testApplicationErrorNoTx_remote() throws Exception {
      EntityExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create(getName());

         exceptionTester.applicationErrorNoTx();

         fail("Expected RemoteException to be thrown");

      } catch(RemoteException e) {
         // good this was expected
         assertNotNull(
               "RemoteException.detail should not be null",
               e.detail);

         assertEquals(
               "RemoteException.detail should be a ApplicationError",
               ApplicationError.class,
               e.detail.getClass());
      } catch(Exception e) {
         fail("Expected RemoteException but got " + e);
      }
   }

   public void testDiscardedApplicationErrorNoTx_remote() throws Exception
   {
      boolean caught = false;
      try
      {
        exceptionTesterHome.findByPrimaryKey("testApplicationErrorNoTx_remote"); 
      }
      catch (FinderException expected)
      {
         caught = true;
      }
      assertTrue("Instance not discarded " + getName(), caught);
   }

   public void testEJBExceptionNoTx_remote() throws Exception {
      EntityExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create(getName());

         exceptionTester.ejbExceptionNoTx();

         fail("Expected RemoteException to be thrown");

      } catch(RemoteException e) {
         // good this was expected
         assertNotNull(
               "RemoteException.detail should not be null",
               e.detail);

         assertEquals(
               "RemoteException.detail should be a EJBException",
               EJBException.class,
               e.detail.getClass());
      } catch(Exception e) {
         fail("Expected RemoteException but got " + e);
      }
   }

   public void testDiscardedEJBExceptionNoTx_remote() throws Exception
   {
      boolean caught = false;
      try
      {
        exceptionTesterHome.findByPrimaryKey("testEJBExceptionNoTx_remote"); 
      }
      catch (FinderException expected)
      {
         caught = true;
      }
      assertTrue("Instance not discarded " + getName(), caught);
   }

   public void testRuntimeExceptionNoTx_remote() throws Exception {
      EntityExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create(getName());

         exceptionTester.runtimeExceptionNoTx();

         fail("Expected RemoteException to be thrown");

      } catch(RemoteException e) {
         // good this was expected
         assertNotNull(
               "RemoteException.detail should not be null",
               e.detail);

         assertEquals(
               "RemoteException.detail should be a RuntimeException",
               RuntimeException.class,
               e.detail.getClass());

      } catch(Exception e) {
         fail("Expected RemoteException but got " + e);
      }
   }

   public void testDiscardedRuntimeExceptionNoTx_remote() throws Exception
   {
      boolean caught = false;
      try
      {
        exceptionTesterHome.findByPrimaryKey("testRuntimeExceptionNoTx_remote"); 
      }
      catch (FinderException expected)
      {
         caught = true;
      }
      assertTrue("Instance not discarded " + getName(), caught);
   }

   public void testRemoteExceptionNoTx_remote() throws Exception {
      EntityExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create(getName());

         exceptionTester.remoteExceptionNoTx();

         fail("Expected RemoteException to be thrown");

      } catch(RemoteException e) {
         // good this was expected
      } catch(Exception e) {
         fail("Expected RemoteException but got " + e);
      }
   }

   public void testDiscardedRemoteExceptionNoTx_remote() throws Exception
   {
      boolean caught = false;
      try
      {
        exceptionTesterHome.findByPrimaryKey("testRemoteExceptionNoTx_remote"); 
      }
      catch (FinderException expected)
      {
         caught = true;
      }
      assertTrue("Instance not discarded " + getName(), caught);
   }

   public void testApplicationExceptionInTx_local() throws Exception {
      EntityExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create(getName());

         exceptionTester.applicationExceptionInTx();

         fail("Expected ApplicationException to be thrown");

      } catch(ApplicationException e) {
         // good this was expected
      } catch(Exception e) {
         fail("Expected ApplicationException but got " + e);
      }
   }

   public void testNotDiscardedApplicationExceptionInTx_local() throws Exception
   {
      exceptionTesterHome.findByPrimaryKey("testApplicationExceptionInTx_local"); 
   }

   public void testApplicationExceptionInTxMarkRollback_local() throws Exception {
      EntityExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create(getName());

         exceptionTester.applicationExceptionInTxMarkRollback();

         fail("Expected ApplicationException to be thrown");

      } catch(ApplicationException e) {
         // good this was expected
      } catch(Exception e) {
         fail("Expected ApplicationException but got " + e);
      }
   }

   public void testNotDiscardedApplicationExceptionInTxMarkRollback_local() throws Exception
   {
      exceptionTesterHome.findByPrimaryKey("testApplicationExceptionInTxMarkRollback_local"); 
   }

   public void testApplicationErrorInTx_local() throws Exception {
      EntityExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create(getName());

         exceptionTester.applicationErrorInTx();

         fail("Expected TransactionRolledbackLocalException to be thrown");

      } catch(TransactionRolledbackLocalException e) {
         // good this was expected
         assertNotNull(
               "TransactionRolledbackLocalException.getCausedByException() " +
                     "should not be null",
               e.getCausedByException());

         assertEquals(
               "TransactionRolledbackLocalException.getCausedByExcption() " +
                     "should be an EJBException",
               EJBException.class,
               e.getCausedByException().getClass());
      } catch(Exception e) {
         fail("Expected TransactionRolledbackLocalException but got " + e);
      }
   }

   public void testDiscardedApplicationErrorInTx_local() throws Exception
   {
      boolean caught = false;
      try
      {
        exceptionTesterHome.findByPrimaryKey("testApplicationErrorInTx_local"); 
      }
      catch (FinderException expected)
      {
         caught = true;
      }
      assertTrue("Instance not discarded " + getName(), caught);
   }

   public void testEJBExceptionInTx_local() throws Exception {
      EntityExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create(getName());

         exceptionTester.ejbExceptionInTx();

         fail("Expected TransactionRolledbackLocalException to be thrown");

      } catch(TransactionRolledbackLocalException e) {
         // good this was expected
         assertNotNull(
               "TransactionRolledbackLocalException.getCausedByException() " +
                     "should not be null",
               e.getCausedByException());

         assertEquals(
               "TransactionRolledbackLocalException.getCausedByException() " +
                     "should be an EJBException",
               EJBException.class,
               e.getCausedByException().getClass());

      } catch(Exception e) {
         fail("Expected TransactionRolledbackLocalException but got " + e);
      }
   }

   public void testDiscardedEJBExceptionInTx_local() throws Exception
   {
      boolean caught = false;
      try
      {
        exceptionTesterHome.findByPrimaryKey("testEJBExceptionInTx_local"); 
      }
      catch (FinderException expected)
      {
         caught = true;
      }
      assertTrue("Instance not discarded " + getName(), caught);
   }

   public void testRuntimeExceptionInTx_local() throws Exception {
      EntityExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create(getName());

         exceptionTester.runtimeExceptionInTx();

         fail("Expected TransactionRolledbackLocalException to be thrown");

      } catch(TransactionRolledbackLocalException e) {
         // good this was expected
         assertNotNull(
               "TransactionRolledbackLocalException.getCausedByException() " +
                     "should not be null",
               e.getCausedByException());

         assertEquals(
               "TransactionRolledbackLocalException.getCausedByException() " + 
                     "should be a RuntimeException",
               RuntimeException.class,
               e.getCausedByException().getClass());

      } catch(Exception e) {
         fail("Expected TransactionRolledbackLocalException but got " + e);
      }
   }

   public void testDiscardedRuntimeExceptionInTx_local() throws Exception
   {
      boolean caught = false;
      try
      {
        exceptionTesterHome.findByPrimaryKey("testRuntimeExceptionInTx_local"); 
      }
      catch (FinderException expected)
      {
         caught = true;
      }
      assertTrue("Instance not discarded " + getName(), caught);
   }

   public void testApplicationExceptionNewTx_local() throws Exception {
      EntityExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create(getName());

         exceptionTester.applicationExceptionNewTx();

         fail("Expected ApplicationException to be thrown");

      } catch(ApplicationException e) {
         // good this was expected
      } catch(Exception e) {
         fail("Expected ApplicationException but got " + e);
      }
   }

   public void testNotDiscardedApplicationExceptionNewTx_local() throws Exception
   {
      exceptionTesterHome.findByPrimaryKey("testApplicationExceptionNewTx_local"); 
   }

   public void testApplicationExceptionNewTxMarkRollback_local() throws Exception {
      EntityExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create(getName());

         exceptionTester.applicationExceptionNewTxMarkRollback();

         fail("Expected ApplicationException to be thrown");

      } catch(ApplicationException e) {
         // good this was expected
      } catch(Exception e) {
         fail("Expected ApplicationException but got " + e);
      }
   }

   public void testNotDiscardedApplicationExceptionNewTxMarkRollback_local() throws Exception
   {
      exceptionTesterHome.findByPrimaryKey("testApplicationExceptionNewTxMarkRollback_local"); 
   }

   public void testApplicationErrorNewTx_local() throws Exception {
      EntityExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create(getName());

         exceptionTester.applicationErrorNewTx();

         fail("Expected EJBException to be thrown");

      } catch(EJBException e) {
         // good this was expected
         assertNull(
               "EJBException.getCausedByException() should be null",
               e.getCausedByException());
      } catch(Exception e) {
         fail("Expected EJBException but got " + e);
      }
   }

   public void testDiscardedApplicationErrorNewTx_local() throws Exception
   {
      boolean caught = false;
      try
      {
        exceptionTesterHome.findByPrimaryKey("testApplicationErrorNewTx_local"); 
      }
      catch (FinderException expected)
      {
         caught = true;
      }
      assertTrue("Instance not discarded " + getName(), caught);
   }

   public void testEJBExceptionNewTx_local() throws Exception {
      EntityExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create(getName());

         exceptionTester.ejbExceptionNewTx();

         fail("Expected EJBException to be thrown");

      } catch(EJBException e) {
         // good this was expected
         assertNull(
               "EJBException.getCausedByException() should be null",
               e.getCausedByException());
      } catch(Exception e) {
         fail("Expected EJBException but got " + e);
      }
   }

   public void testDiscardedEJBExceptionNewTx_local() throws Exception
   {
      boolean caught = false;
      try
      {
        exceptionTesterHome.findByPrimaryKey("testEJBExceptionNewTx_local"); 
      }
      catch (FinderException expected)
      {
         caught = true;
      }
      assertTrue("Instance not discarded " + getName(), caught);
   }

   public void testRuntimeExceptionNewTx_local() throws Exception {
      EntityExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create(getName());

         exceptionTester.runtimeExceptionNewTx();

         fail("Expected EJBException to be thrown");

      } catch(EJBException e) {
         // good this was expected
         assertNotNull(
               "EJBException.getCausedByException() should not be null",
               e.getCausedByException());

         assertEquals(
               "EJBException.getCausedByException() should be " +
                     "a RuntimeException",
               RuntimeException.class,
               e.getCausedByException().getClass());

      } catch(Exception e) {
         fail("Expected EJBException but got " + e);
      }
   }

   public void testDiscardedRuntimeExceptionNewTx_local() throws Exception
   {
      boolean caught = false;
      try
      {
        exceptionTesterHome.findByPrimaryKey("testRuntimeExceptionNewTx_local"); 
      }
      catch (FinderException expected)
      {
         caught = true;
      }
      assertTrue("Instance not discarded " + getName(), caught);
   }

   public void testApplicationExceptionNoTx_local() throws Exception {
      EntityExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create(getName());

         exceptionTester.applicationExceptionNoTx();

         fail("Expected application exception to be thrown");

      } catch(ApplicationException e) {
         // good this was expected
      } catch(Exception e) {
         fail("Expected ApplicationException but got " + e);
      }
   }

   public void testNotDiscardedApplicationExceptionNoTx_local() throws Exception
   {
      exceptionTesterHome.findByPrimaryKey("testApplicationExceptionNoTx_local"); 
   }

   public void testApplicationErrorNoTx_local() throws Exception {
      EntityExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create(getName());

         exceptionTester.applicationErrorNoTx();

         fail("Expected EJBException to be thrown");

      } catch(EJBException e) {
         // good this was expected
         assertNull(
               "EJBException.getCausedByException() should be null",
               e.getCausedByException());
      } catch(Exception e) {
         fail("Expected EJBException but got " + e);
      }
   }

   public void testDiscardedApplicationErrorNoTx_local() throws Exception
   {
      boolean caught = false;
      try
      {
        exceptionTesterHome.findByPrimaryKey("testApplicationErrorNoTx_local"); 
      }
      catch (FinderException expected)
      {
         caught = true;
      }
      assertTrue("Instance not discarded " + getName(), caught);
   }

   public void testEJBExceptionNoTx_local() throws Exception {
      EntityExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create(getName());

         exceptionTester.ejbExceptionNoTx();

         fail("Expected EJBException to be thrown");

      } catch(EJBException e) {
         // good this was expected
         assertNull(
               "EJBException.getCausedByException() should be null",
               e.getCausedByException());
      } catch(Exception e) {
         fail("Expected EJBException but got " + e);
      }
   }

   public void testDiscardedEJBExceptionNoTx_local() throws Exception
   {
      boolean caught = false;
      try
      {
        exceptionTesterHome.findByPrimaryKey("testEJBExceptionNoTx_local"); 
      }
      catch (FinderException expected)
      {
         caught = true;
      }
      assertTrue("Instance not discarded " + getName(), caught);
   }

   public void testRuntimeExceptionNoTx_local() throws Exception {
      EntityExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create(getName());

         exceptionTester.runtimeExceptionNoTx();

         fail("Expected EJBException to be thrown");

      } catch(EJBException e) {
         // good this was expected
         assertNotNull(
               "EJBException.getCausedByException() should not be null",
               e.getCausedByException());

         assertEquals(
               "EJBException.getCausedByException() should be " +
                     "a RuntimeException",
               RuntimeException.class,
               e.getCausedByException().getClass());

      } catch(Exception e) {
         fail("Expected EJBException but got " + e);
      }
   }

   public void testDiscardedRuntimeExceptionNoTx_local() throws Exception
   {
      boolean caught = false;
      try
      {
        exceptionTesterHome.findByPrimaryKey("testRuntimeExceptionNoTx_local"); 
      }
      catch (FinderException expected)
      {
         caught = true;
      }
      assertTrue("Instance not discarded " + getName(), caught);
   }
}
