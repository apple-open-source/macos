package org.jboss.test.exception;

import java.rmi.RemoteException;
import java.util.Collection;
import java.util.HashSet;
import java.util.Set;
import java.util.Iterator;
import javax.naming.InitialContext;
import javax.ejb.EJBException;
import javax.ejb.EJBLocalObject;
import javax.ejb.TransactionRolledbackLocalException;
import javax.transaction.TransactionRolledbackException;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;
import org.apache.log4j.Category;
import net.sourceforge.junitejb.EJBTestCase;

import org.jboss.test.JBossTestCase;

public class ExceptionUnitTestCase extends EJBTestCase {
   public static Test suite() throws Exception {
		return JBossTestCase.getDeploySetup(
            ExceptionUnitTestCase.class, 
            "exception.jar");
   }   

   public ExceptionUnitTestCase(String name) {
      super(name);
   }

   private Category log = Category.getInstance(getClass());
   private ExceptionTesterHome exceptionTesterHome;
   private ExceptionTesterLocalHome exceptionTesterLocalHome;

   /**
    * Looks up all of the home interfaces and creates the initial data. 
    * Looking up objects in JNDI is expensive, so it should be done once 
    * and cached.
    * @throws Exception if a problem occures while finding the home interfaces,
    * or if an problem occures while createing the initial data
    */
   public void setUp() throws Exception {
      InitialContext jndi = new InitialContext();

      exceptionTesterHome = (ExceptionTesterHome) 
            jndi.lookup("exception/ExceptionTester"); 

      exceptionTesterLocalHome = (ExceptionTesterLocalHome) 
            jndi.lookup("exception/ExceptionTesterLocal"); 
   }

   public void testApplicationExceptionInTx_remote() throws Exception {
      ExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create();

         exceptionTester.applicationExceptionInTx();

         fail("Expected application exception to be thrown");

      } catch(ApplicationException e) {
         // good this was expected
      } catch(Exception e) {
         fail("Expected ApplicationException but got " + e);
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }

   public void testApplicationErrorInTx_remote() throws Exception {
      ExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create();

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
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }


   public void testEJBExceptionInTx_remote() throws Exception {
      ExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create();

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
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }


   public void testRuntimeExceptionInTx_remote() throws Exception {
      ExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create();

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
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }


   public void testRemoteExceptionInTx_remote() throws Exception {
      ExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create();

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
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }

   public void testApplicationExceptionNewTx_remote() throws Exception {
      ExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create();

         exceptionTester.applicationExceptionNewTx();

         fail("Expected application exception to be thrown");

      } catch(ApplicationException e) {
         // good this was expected
      } catch(Exception e) {
         fail("Expected ApplicationException but got " + e);
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }

   public void testApplicationErrorNewTx_remote() throws Exception {
      ExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create();

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
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }

   public void testEJBExceptionNewTx_remote() throws Exception {
      ExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create();

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
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }


   public void testRuntimeExceptionNewTx_remote() throws Exception {
      ExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create();

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
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }


   public void testRemoteExceptionNewTx_remote() throws Exception {
      ExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create();

         exceptionTester.remoteExceptionNewTx();

         fail("Expected RemoteException to be thrown");

      } catch(RemoteException e) {
         // good this was expected
      } catch(Exception e) {
         fail("Expected RemoteException but got " + e);
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }

   public void testApplicationExceptionNoTx_remote() throws Exception {
      ExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create();

         exceptionTester.applicationExceptionNoTx();

         fail("Expected application exception to be thrown");

      } catch(ApplicationException e) {
         // good this was expected
      } catch(Exception e) {
         fail("Expected ApplicationException but got " + e);
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }

   public void testApplicationErrorNoTx_remote() throws Exception {
      ExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create();

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
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }

   public void testEJBExceptionNoTx_remote() throws Exception {
      ExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create();

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
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }


   public void testRuntimeExceptionNoTx_remote() throws Exception {
      ExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create();

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
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }


   public void testRemoteExceptionNoTx_remote() throws Exception {
      ExceptionTester exceptionTester = null;
      try {
         exceptionTester = exceptionTesterHome.create();

         exceptionTester.remoteExceptionNoTx();

         fail("Expected RemoteException to be thrown");

      } catch(RemoteException e) {
         // good this was expected
      } catch(Exception e) {
         fail("Expected RemoteException but got " + e);
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }




   public void testApplicationExceptionInTx_local() throws Exception {
      ExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create();

         exceptionTester.applicationExceptionInTx();

         fail("Expected ApplicationException to be thrown");

      } catch(ApplicationException e) {
         // good this was expected
      } catch(Exception e) {
         fail("Expected ApplicationException but got " + e);
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }

   public void testApplicationErrorInTx_local() throws Exception {
      ExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create();

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
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }


   public void testEJBExceptionInTx_local() throws Exception {
      ExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create();

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
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }


   public void testRuntimeExceptionInTx_local() throws Exception {
      ExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create();

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
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }

   public void testApplicationExceptionNewTx_local() throws Exception {
      ExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create();

         exceptionTester.applicationExceptionNewTx();

         fail("Expected ApplicationException to be thrown");

      } catch(ApplicationException e) {
         // good this was expected
      } catch(Exception e) {
         fail("Expected ApplicationException but got " + e);
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }

   public void testApplicationErrorNewTx_local() throws Exception {
      ExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create();

         exceptionTester.applicationErrorNewTx();

         fail("Expected EJBException to be thrown");

      } catch(EJBException e) {
         // good this was expected
         assertNull(
               "EJBException.getCausedByException() should be null",
               e.getCausedByException());
      } catch(Exception e) {
         fail("Expected EJBException but got " + e);
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }

   public void testEJBExceptionNewTx_local() throws Exception {
      ExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create();

         exceptionTester.ejbExceptionNewTx();

         fail("Expected EJBException to be thrown");

      } catch(EJBException e) {
         // good this was expected
         assertNull(
               "EJBException.getCausedByException() should be null",
               e.getCausedByException());
      } catch(Exception e) {
         fail("Expected EJBException but got " + e);
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }


   public void testRuntimeExceptionNewTx_local() throws Exception {
      ExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create();

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
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }

   public void testApplicationExceptionNoTx_local() throws Exception {
      ExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create();

         exceptionTester.applicationExceptionNoTx();

         fail("Expected application exception to be thrown");

      } catch(ApplicationException e) {
         // good this was expected
      } catch(Exception e) {
         fail("Expected ApplicationException but got " + e);
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }

   public void testApplicationErrorNoTx_local() throws Exception {
      ExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create();

         exceptionTester.applicationErrorNoTx();

         fail("Expected EJBException to be thrown");

      } catch(EJBException e) {
         // good this was expected
         assertNull(
               "EJBException.getCausedByException() should be null",
               e.getCausedByException());
      } catch(Exception e) {
         fail("Expected EJBException but got " + e);
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }

   public void testEJBExceptionNoTx_local() throws Exception {
      ExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create();

         exceptionTester.ejbExceptionNoTx();

         fail("Expected EJBException to be thrown");

      } catch(EJBException e) {
         // good this was expected
         assertNull(
               "EJBException.getCausedByException() should be null",
               e.getCausedByException());
      } catch(Exception e) {
         fail("Expected EJBException but got " + e);
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }


   public void testRuntimeExceptionNoTx_local() throws Exception {
      ExceptionTesterLocal exceptionTester = null;
      try {
         exceptionTester = exceptionTesterLocalHome.create();

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
      } finally {
         if(exceptionTester != null) {
            exceptionTester.remove();
         }
      }
   }
}
