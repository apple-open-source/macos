package org.jboss.test.cts.interfaces;


import javax.ejb.*;

import javax.transaction.Status;
import javax.transaction.UserTransaction;
import javax.transaction.RollbackException;

import org.jboss.test.cts.keys.AccountPK;

/**
 *  A utility class for testing UserTransaction in a stand-alone
 *  client, or a BMT enterprise bean.
 *  This is not part of any interface, just shared code used in
 *  client and server.
 *  This does not depend on JUnit, as JUnit is not available on the server.
 *
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.2.4.1 $
 */
public class UserTransactionTester
{
   static org.apache.log4j.Category log =
      org.apache.log4j.Category.getInstance(UserTransactionTester.class);

   /**
    *  Home of entity used as a resource for testing.
    */
   private CtsBmpHome home;

   /**
    *  UserTransaction to test.
    */
   private UserTransaction ut;

   /**
    *  First resource for testing.
    */
   private CtsBmp bean1;

   /**
    *  Second resource for testing.
    */
   private CtsBmp bean2;

   /**
    *  Create a new UserTransaction test instance.
    */
   public UserTransactionTester(CtsBmpHome home,
                                UserTransaction userTransaction)
   {
      this.home = home;
      this.ut = userTransaction;
   }

   /**
    *  Run all the UserTransaction tests.
    */
   public boolean runAllTests()
   {
      // No resource tests
      if (!testBeginRollback())
         return false;
      if (!testBeginCommit())
         return false;
      if (!testBeginSetrollbackonlyRollback())
         return false;
      if (!testBeginSetrollbackonlyCommit())
         return false;

      // Create first instance
      try {
         bean1 = home.create(new AccountPK("UT_TestBean1"), "Ole1");
      } catch (Exception ex) {
         log.debug("failed", ex);
         return false;
      }

      // Single resource tests
      if (!testSingleRollback())
         return false;
      if (!testSingleCommit())
         return false;
      if (!testSingleSetrollbackonlyCommit())
         return false;

      // Can second instance be created in a tx that is rolled back?
      try {
         ut.begin();
         bean2 = home.create(new AccountPK("UT_TestBean2"), "Ole2");
         ut.rollback();
         
         // Should no longer exist
         boolean gotException = false;
         try {
            bean2.setPersonsName("Ole");
         } catch (Exception e) {
	   log.info("IGNORE PREVIOUS NoSuchEntityException - it is intentional");
            gotException = true;
         }
         if (!gotException)
            throw new RuntimeException("Rollback didn't rollback create.");
      } catch (Exception ex) {
         log.debug("failed", ex);
         return false;
      }

      // Create second instance
      try {
         bean2 = home.create(new AccountPK("UT_TestBean2"), "Ole2");
      } catch (Exception ex) {
         log.debug("failed", ex);
         return false;
      }

      return true;
   }

   //
   //  No resource tests.
   //

   /**
    *  Simple begin/rollback test.
    */
   private boolean testBeginRollback()
   {
      try {
         if (ut.getStatus() != Status.STATUS_NO_TRANSACTION)
            throw new RuntimeException("No tx should be active.");

         ut.begin();
         if (ut.getStatus() != Status.STATUS_ACTIVE)
            throw new RuntimeException("New tx not active.");
         ut.rollback();

         if (ut.getStatus() != Status.STATUS_NO_TRANSACTION)
            throw new RuntimeException("No tx should be active.");
      } catch (Exception ex) {
         log.debug("failed", ex);
         return false;
      }
      return true;
   }

   /**
    *  Simple begin/commit test.
    */
   private boolean testBeginCommit()
   {
      try {
         if (ut.getStatus() != Status.STATUS_NO_TRANSACTION)
            throw new RuntimeException("No tx should be active.");

         ut.begin();
         if (ut.getStatus() != Status.STATUS_ACTIVE)
            throw new RuntimeException("New tx not active.");
         ut.commit();

         if (ut.getStatus() != Status.STATUS_NO_TRANSACTION)
            throw new RuntimeException("No tx should be active.");
      } catch (Exception ex) {
         log.debug("failed", ex);
         return false;
      }
      return true;
   }

   /**
    *  Simple begin/setRollbackOnly/rollback test.
    */
   private boolean testBeginSetrollbackonlyRollback()
   {
      try {
         if (ut.getStatus() != Status.STATUS_NO_TRANSACTION)
            throw new RuntimeException("No tx should be active.");

         ut.begin();
         if (ut.getStatus() != Status.STATUS_ACTIVE)
            throw new RuntimeException("New tx not active.");
         ut.setRollbackOnly();
         if (ut.getStatus() != Status.STATUS_MARKED_ROLLBACK)
            throw new RuntimeException("Tx not marked for rollback.");
         ut.rollback();

         if (ut.getStatus() != Status.STATUS_NO_TRANSACTION)
            throw new RuntimeException("No tx should be active.");
      } catch (Exception ex) {
         log.debug("failed", ex);
         return false;
      }
      return true;
   }

   /**
    *  Simple begin/setRollbackOnly/commit test.
    */
   private boolean testBeginSetrollbackonlyCommit()
   {
      try {
         if (ut.getStatus() != Status.STATUS_NO_TRANSACTION)
            throw new RuntimeException("No tx should be active.");

         ut.begin();
         if (ut.getStatus() != Status.STATUS_ACTIVE)
            throw new RuntimeException("New tx not active.");
         ut.setRollbackOnly();
         if (ut.getStatus() != Status.STATUS_MARKED_ROLLBACK)
            throw new RuntimeException("Tx not marked for rollback.");

         boolean gotException = false;
         try {
            ut.commit();
         } catch (RollbackException rbe) {
            gotException = true;
         }
         if (!gotException)
            throw new RuntimeException("Didn't get expected RollbackException.");

         if (ut.getStatus() != Status.STATUS_NO_TRANSACTION)
            throw new RuntimeException("No tx should be active.");
      } catch (Exception ex) {
         log.debug("failed", ex);
         return false;
      }
      return true;
   }


   //
   //  Single resource tests.
   //

   /**
    *  Tests if a rollback really rolls back.
    */
   private boolean testSingleRollback()
   {
      try {
         if (ut.getStatus() != Status.STATUS_NO_TRANSACTION)
            throw new RuntimeException("No tx should be active.");

         bean1.setPersonsName("Ole");
         if (!bean1.getPersonsName().equals("Ole"))
            throw new RuntimeException("Unable to set property.");

         ut.begin();
         if (ut.getStatus() != Status.STATUS_ACTIVE)
            throw new RuntimeException("New tx not active.");
         if (!bean1.getPersonsName().equals("Ole"))
            throw new RuntimeException("Property changes after begin.");

         bean1.setPersonsName("Peter");
         if (!bean1.getPersonsName().equals("Peter"))
             throw new RuntimeException("Unable to set property.");

         ut.rollback();
         if (!bean1.getPersonsName().equals("Ole"))
             throw new RuntimeException("Rollback doesn't work.");


         if (ut.getStatus() != Status.STATUS_NO_TRANSACTION)
            throw new RuntimeException("No tx should be active.");
      } catch (Exception ex) {
         log.debug("failed", ex);
         return false;
      }
      return true;
   }

   /**
    *  Tests if a commit really commits.
    */
   private boolean testSingleCommit()
   {
      try {
         if (ut.getStatus() != Status.STATUS_NO_TRANSACTION)
            throw new RuntimeException("No tx should be active.");

         bean1.setPersonsName("Ole");
         if (!bean1.getPersonsName().equals("Ole"))
            throw new RuntimeException("Unable to set property.");

         ut.begin();
         if (ut.getStatus() != Status.STATUS_ACTIVE)
            throw new RuntimeException("New tx not active.");
         if (!bean1.getPersonsName().equals("Ole"))
            throw new RuntimeException("Property changes after begin.");

         bean1.setPersonsName("Peter");
         if (!bean1.getPersonsName().equals("Peter"))
             throw new RuntimeException("Unable to set property.");

         ut.commit();
         if (!bean1.getPersonsName().equals("Peter"))
             throw new RuntimeException("Property not set after commit.");


         if (ut.getStatus() != Status.STATUS_NO_TRANSACTION)
            throw new RuntimeException("No tx should be active.");
      } catch (Exception ex) {
         log.debug("failed", ex);
         return false;
      }
      return true;
   }

   /**
    *  Tests if a setRollbackOnly really makes the transaction rollback.
    */
   private boolean testSingleSetrollbackonlyCommit()
   {
      try {
         if (ut.getStatus() != Status.STATUS_NO_TRANSACTION)
            throw new RuntimeException("No tx should be active.");

         bean1.setPersonsName("Ole");
         if (!bean1.getPersonsName().equals("Ole"))
            throw new RuntimeException("Unable to set property.");

         ut.begin();
         if (ut.getStatus() != Status.STATUS_ACTIVE)
            throw new RuntimeException("New tx not active.");
         if (!bean1.getPersonsName().equals("Ole"))
            throw new RuntimeException("Property changes after begin.");

         bean1.setPersonsName("Peter");
         if (!bean1.getPersonsName().equals("Peter"))
             throw new RuntimeException("Unable to set property.");

         ut.setRollbackOnly();
         if (ut.getStatus() != Status.STATUS_MARKED_ROLLBACK)
            throw new RuntimeException("Tx not marked for rollback.");

         boolean gotException = false;
         try {
            ut.commit();
         } catch (RollbackException rbe) {
            gotException = true;
         }
         if (!gotException)
            throw new RuntimeException("Didn't get expected RollbackException.");

         if (!bean1.getPersonsName().equals("Ole"))
             throw new RuntimeException("Didn't roll back.");

         if (ut.getStatus() != Status.STATUS_NO_TRANSACTION)
            throw new RuntimeException("No tx should be active.");
      } catch (Exception ex) {
         log.debug("failed", ex);
         return false;
      }
      return true;
   }
}
