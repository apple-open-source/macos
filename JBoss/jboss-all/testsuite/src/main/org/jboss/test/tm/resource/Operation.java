/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.tm.resource;

import java.io.Serializable;
import java.util.HashMap;

import javax.naming.InitialContext;
import javax.transaction.Transaction;
import javax.transaction.TransactionManager;

import org.jboss.logging.Logger;

/**
 * Operations
 * @author Adrian@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class Operation
   implements Serializable
{
   public static final int BEGIN = -1;
   public static final int COMMIT = -2;
   public static final int ROLLBACK = -3;
   public static final int SUSPEND = -4;
   public static final int RESUME = -5;
   public static final int SETROLLBACK = -6;
   public static final int STATUS = -7;
   public static final int STATE = 0;
   public static final int CREATE = 1;
   public static final int ENLIST = 2;
   public static final int DIFFRM = 3;
   public static final int SETSTATUS = 4;

   static HashMap resources = new HashMap();
   static HashMap transactions = new HashMap();

   static Logger log;

   static TransactionManager tm = null;

   Integer id;
   int op;
   int status;
   Throwable throwable;

   public Operation(int op, int id)
   {
      this(op, id, 0);
   }

   public Operation(int op, int id, int status)
   {
      this(op, id, status, null);
   }

   public Operation(int op, int id, int status, Throwable throwable)
   {
      this.id = new Integer(id);
      this.op = op;
      this.status = status;
      this.throwable = throwable;
   }

   public void perform() throws Exception
   {
      Throwable caught = null;
      try
      {
         switch (op)
         {
         case BEGIN:
            begin();
            break;
         case COMMIT:
            commit();
            break;
         case ROLLBACK:
            rollback();
            break;
         case SUSPEND:
            suspend();
            break;
         case RESUME:
            resume();
            break;
         case SETROLLBACK:
            setRollbackOnly();
            break;
         case STATUS:
            checkStatus();
            break;
         case STATE:
            checkState();
            break;
         case CREATE:
            create();
            break;
         case ENLIST:
            enlist();
            break;
         case DIFFRM:
            differentRM();
            break;
         case SETSTATUS:
            setStatus();
            break;
         default:
            throw new IllegalArgumentException("Invalid operation " + op);
         }
      }
      catch (Throwable t)
      {
         caught = t;
      }
      if (throwable != null && caught == null)
         throw new Exception("Expected throwable " + throwable);
      if (throwable != null && (throwable.getClass().isAssignableFrom(caught.getClass())) == false)
      {
         caught.printStackTrace();
         throw new Exception("Expected throwable " + throwable + " was " + caught);
      }
      if (throwable == null && caught != null)
      {
         caught.printStackTrace();
         throw new Exception("Unexpected throwable " + caught);
      }
   }

   public void begin() throws Exception
   {
      log.info("BEGIN " + id);
      getTM().begin();
      Transaction tx = getTM().getTransaction();
      transactions.put(id, tx);
      log.info("BEGUN " + tx);
   }

   public void commit() throws Exception
   {
      log.info("COMMIT " + id);
      assertTx(id);
      getTM().commit();
      log.info("COMMITTED " + id);
   }

   public void rollback() throws Exception
   {
      log.info("ROLLBACK " + id);
      assertTx(id);
      getTM().rollback();
      log.info("ROLLEDBACK " + id);
   }

   public void suspend() throws Exception
   {
      log.info("SUSPEND " + id);
      assertTx(id);
      getTM().suspend();
      log.info("SUSPENDED " + id);
   }

   public void resume() throws Exception
   {
      log.info("RESUME " + id);
      getTM().resume(getTx(id));
      assertTx(id);
      log.info("RESUMED " + id);
   }

   public void setRollbackOnly() throws Exception
   {
      log.info("SETROLLBACK " + id);
      getTx(id).setRollbackOnly();
      log.info("SETTEDROLLBACK " + id);
   }

   public void checkStatus() throws Exception
   {
      log.info("CHECKSTATUS " + id);
      int actualStatus = getTx(id).getStatus();
      log.info("CHECKINGSTATUS " + id + " Expected " + status + " was " + actualStatus);
      if (actualStatus != status)
         throw new Exception("Transaction " + id + " Expected status " + status + " was " + actualStatus);
   }

   public void checkState() throws Exception
   {
      log.info("CHECKSTATE " + id);
      int actualStatus = getRes(id).getStatus();
      log.info("CHECKINGSTATE " + id + " Expected " + status + " was " + actualStatus);
      if (actualStatus != status)
         throw new Exception("Resource " + id + " Expected state " + status + " was " + actualStatus);
   }

   public void create() throws Exception
   {
      log.info("CREATE " + id);
      Resource res = new Resource(id);
      resources.put(id, res);
      log.info("CREATED " + res);
   }

   public void enlist() throws Exception
   {
      log.info("ENLIST " + id);
      Transaction tx = getTM().getTransaction();
      if (tx.enlistResource(getRes(id)) == false)
         throw new Exception("Unable to enlist resource");
      log.info("ENLISTED " + id + " " + tx);
   }

   public void differentRM() throws Exception
   {
      log.info("DIFFRM " + id);
      getRes(id).newResourceManager();
   }

   public void setStatus() throws Exception
   {
      log.info("SETSTATUS " + id + " " + status);
      getRes(id).setStatus(status);
      log.info("SETTEDSTATUS " + id + " " + status);
   }

   public static void start(Logger log)
      throws Exception
   {
      Operation.log = log;
      if (getTM().getTransaction() != null)
         throw new IllegalStateException("Invalid thread association " + getTM().getTransaction());
      reset();
   }

   public static void end()
   {
      reset();
   }

   public static void reset()
   {
      resources.clear();
      transactions.clear();
   }

   public Resource getRes(Integer id)
   {
      Resource res = (Resource) resources.get(id);
      if (res == null)
         throw new IllegalStateException("No resource: " + id);
      return res;
   }
   public Transaction getTx(Integer id)
   {
      Transaction tx = (Transaction) transactions.get(id);
      if (tx == null)
         throw new IllegalStateException("No transaction: " + id);
      return tx;
   }
 
   public void assertTx(Integer id)
      throws Exception
   {
      Transaction tx = getTx(id);
      Transaction current = getTM().getTransaction();
      log.info("Asserting tx " + tx + " current " + current);
      if (tx.equals(current) == false)
         throw new IllegalStateException("Expected tx " + tx + " was " + current);
   }

   public static TransactionManager getTM()
      throws Exception
   {
      if (tm == null)
         tm = (TransactionManager) new InitialContext().lookup("java:/TransactionManager");
      return tm;
   }
}
