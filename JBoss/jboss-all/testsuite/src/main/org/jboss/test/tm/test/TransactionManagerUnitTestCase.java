/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.tm.test;

import javax.management.ObjectName;
import javax.transaction.RollbackException;
import javax.transaction.Status;
import javax.transaction.xa.XAException;

import junit.framework.Test;

import org.jboss.test.JBossTestCase;
import org.jboss.test.JBossTestSetup;
import org.jboss.test.tm.resource.Operation;
import org.jboss.test.tm.resource.Resource;

/**
 * Tests for the transaction manager
 * @author Adrian@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class TransactionManagerUnitTestCase
   extends JBossTestCase
{
   static String[] SIG = new String[] { String.class.getName(), new Operation[0].getClass().getName() };

   ObjectName tmMBean;

   public TransactionManagerUnitTestCase(String name)
   {
      super(name);

      try
      {
         tmMBean = new ObjectName("jboss.test:test=TransactionManagerUnitTestCase");
      }
      catch (Exception e)
      {
         throw new RuntimeException(e.toString());
      }
   }

   public void runTest(Operation[] ops) throws Exception
   {
      getServer().invoke(tmMBean, "testOperations", new Object[] { getName(), ops }, SIG);
   }

   public static Test suite() throws Exception
   {
      return new JBossTestSetup(getDeploySetup(TransactionManagerUnitTestCase.class, "tmtest.sar"));
   }

   public void testNoResourcesCommit() throws Exception
   {
      runTest(new Operation[]
      {
         new Operation(Operation.BEGIN, 1),
         new Operation(Operation.STATUS, 1, Status.STATUS_ACTIVE),
         new Operation(Operation.COMMIT, 1),
         new Operation(Operation.STATUS, 1, Status.STATUS_NO_TRANSACTION)
      });
   }

   public void testNoResourcesRollback() throws Exception
   {
      runTest(new Operation[]
      {
         new Operation(Operation.BEGIN, 1),
         new Operation(Operation.STATUS, 1, Status.STATUS_ACTIVE),
         new Operation(Operation.ROLLBACK, 1),
         new Operation(Operation.STATUS, 1, Status.STATUS_NO_TRANSACTION)
      });
   }

   public void testNoResourcesSuspendResume() throws Exception
   {
      runTest(new Operation[]
      {
         new Operation(Operation.BEGIN, 1),
         new Operation(Operation.STATUS, 1, Status.STATUS_ACTIVE),
         new Operation(Operation.SUSPEND, 1),
         new Operation(Operation.STATUS, 1, Status.STATUS_ACTIVE),
         new Operation(Operation.RESUME, 1),
         new Operation(Operation.STATUS, 1, Status.STATUS_ACTIVE),
         new Operation(Operation.COMMIT, 1),
         new Operation(Operation.STATUS, 1, Status.STATUS_NO_TRANSACTION)
      });
   }

   public void testOneResourceCommit() throws Exception
   {
      runTest(new Operation[]
      {
         new Operation(Operation.BEGIN, 1),
         new Operation(Operation.STATUS, 1, Status.STATUS_ACTIVE),
         new Operation(Operation.CREATE, 1),
         new Operation(Operation.ENLIST, 1),
         new Operation(Operation.STATE, 1, Resource.ACTIVE),
         new Operation(Operation.COMMIT, 1),
         new Operation(Operation.STATE, 1, Resource.COMMITTED),
         new Operation(Operation.STATUS, 1, Status.STATUS_NO_TRANSACTION)
      });
   }

   public void testOneResourceRollback() throws Exception
   {
      runTest(new Operation[]
      {
         new Operation(Operation.BEGIN, 1),
         new Operation(Operation.STATUS, 1, Status.STATUS_ACTIVE),
         new Operation(Operation.CREATE, 1),
         new Operation(Operation.ENLIST, 1),
         new Operation(Operation.STATE, 1, Resource.ACTIVE),
         new Operation(Operation.ROLLBACK, 1),
         new Operation(Operation.STATE, 1, Resource.ROLLEDBACK),
         new Operation(Operation.STATUS, 1, Status.STATUS_NO_TRANSACTION)
      });
   }

   public void testOneResourceSetRollback() throws Exception
   {
      runTest(new Operation[]
      {
         new Operation(Operation.BEGIN, 1),
         new Operation(Operation.STATUS, 1, Status.STATUS_ACTIVE),
         new Operation(Operation.CREATE, 1),
         new Operation(Operation.ENLIST, 1),
         new Operation(Operation.STATE, 1, Resource.ACTIVE),
         new Operation(Operation.SETROLLBACK, 1),
         new Operation(Operation.STATUS, 1, Status.STATUS_MARKED_ROLLBACK),
         new Operation(Operation.COMMIT, 1, 0, new RollbackException()),
         new Operation(Operation.STATE, 1, Resource.ROLLEDBACK),
         new Operation(Operation.STATUS, 1, Status.STATUS_NO_TRANSACTION)
      });
   }

   public void testTwoResourceSameRMCommit() throws Exception
   {
      runTest(new Operation[]
      {
         new Operation(Operation.BEGIN, 1),
         new Operation(Operation.STATUS, 1, Status.STATUS_ACTIVE),
         new Operation(Operation.CREATE, 1),
         new Operation(Operation.ENLIST, 1),
         new Operation(Operation.STATE, 1, Resource.ACTIVE),
         new Operation(Operation.CREATE, 2),
         new Operation(Operation.ENLIST, 2),
         new Operation(Operation.STATE, 2, Resource.ACTIVE),
         new Operation(Operation.COMMIT, 1),
         new Operation(Operation.STATE, 1, Resource.COMMITTED),
         new Operation(Operation.STATE, 2, Resource.COMMITTED),
         new Operation(Operation.STATUS, 1, Status.STATUS_NO_TRANSACTION)
      });
   }

   public void testTwoResourceSameRMRollback() throws Exception
   {
      runTest(new Operation[]
      {
         new Operation(Operation.BEGIN, 1),
         new Operation(Operation.STATUS, 1, Status.STATUS_ACTIVE),
         new Operation(Operation.CREATE, 1),
         new Operation(Operation.ENLIST, 1),
         new Operation(Operation.STATE, 1, Resource.ACTIVE),
         new Operation(Operation.CREATE, 2),
         new Operation(Operation.ENLIST, 2),
         new Operation(Operation.STATE, 2, Resource.ACTIVE),
         new Operation(Operation.ROLLBACK, 1),
         new Operation(Operation.STATE, 1, Resource.ROLLEDBACK),
         new Operation(Operation.STATE, 2, Resource.ROLLEDBACK),
         new Operation(Operation.STATUS, 1, Status.STATUS_NO_TRANSACTION)
      });
   }

   public void testTwoResourceDifferentRMCommit() throws Exception
   {
      runTest(new Operation[]
      {
         new Operation(Operation.BEGIN, 1),
         new Operation(Operation.STATUS, 1, Status.STATUS_ACTIVE),
         new Operation(Operation.CREATE, 1),
         new Operation(Operation.ENLIST, 1),
         new Operation(Operation.STATE, 1, Resource.ACTIVE),
         new Operation(Operation.CREATE, 2),
         new Operation(Operation.ENLIST, 2),
         new Operation(Operation.DIFFRM, 2),
         new Operation(Operation.STATE, 2, Resource.ACTIVE),
         new Operation(Operation.COMMIT, 1),
         new Operation(Operation.STATE, 1, Resource.COMMITTED),
         new Operation(Operation.STATE, 2, Resource.COMMITTED),
         new Operation(Operation.STATUS, 1, Status.STATUS_NO_TRANSACTION)
      });
   }

   public void testTwoResourceDifferentRMRollback() throws Exception
   {
      runTest(new Operation[]
      {
         new Operation(Operation.BEGIN, 1),
         new Operation(Operation.STATUS, 1, Status.STATUS_ACTIVE),
         new Operation(Operation.CREATE, 1),
         new Operation(Operation.ENLIST, 1),
         new Operation(Operation.STATE, 1, Resource.ACTIVE),
         new Operation(Operation.CREATE, 2),
         new Operation(Operation.ENLIST, 2),
         new Operation(Operation.DIFFRM, 2),
         new Operation(Operation.STATE, 2, Resource.ACTIVE),
         new Operation(Operation.ROLLBACK, 1),
         new Operation(Operation.STATE, 1, Resource.ROLLEDBACK),
         new Operation(Operation.STATE, 2, Resource.ROLLEDBACK),
         new Operation(Operation.STATUS, 1, Status.STATUS_NO_TRANSACTION)
      });
   }

   public void testOneResourceCommitHeurRB() throws Exception
   {
      runTest(new Operation[]
      {
         new Operation(Operation.BEGIN, 1),
         new Operation(Operation.STATUS, 1, Status.STATUS_ACTIVE),
         new Operation(Operation.CREATE, 1),
         new Operation(Operation.ENLIST, 1),
         new Operation(Operation.STATE, 1, Resource.ACTIVE),
         new Operation(Operation.SETSTATUS, 1, XAException.XA_HEURRB),
         new Operation(Operation.COMMIT, 1, 0,new RollbackException()),
         new Operation(Operation.STATE, 1, Resource.FORGOT),
         new Operation(Operation.STATUS, 1, Status.STATUS_NO_TRANSACTION)
      });
   }
}
