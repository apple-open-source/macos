/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb;

import org.jboss.logging.Logger;
import org.jboss.tm.TransactionLocal;

import javax.ejb.EJBException;
import javax.transaction.RollbackException;
import javax.transaction.Status;
import javax.transaction.Synchronization;
import javax.transaction.SystemException;
import javax.transaction.Transaction;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

/**
 * This class provides a way to find out what entities are contained in
 * what transaction.  It is used, to find which entities to call ejbStore()
 * on when a ejbFind() method is called within a transaction. EJB 2.0- 9.6.4
 * also, it is used to synchronize on a remove.
 * Used in EntitySynchronizationInterceptor, EntityContainer
 *
 * Entities are stored in an ArrayList to ensure specific ordering.
 *
 * @author <a href="bill@burkecentral.com">Bill Burke</a>
 * @version $Revision: 1.4.2.5 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>20021121 Steve Coy:</b>
 * <ul>
 * <li>Backport David J's fixes from HEAD.
 * <li>Fix a performance bottleneck by adding a parallel map of sets of
 *     entities so that we can check for their existence in the trx in
 *     O(log N) time instead of O(N) time.
 */
public class GlobalTxEntityMap
{

   private final Logger log = Logger.getLogger(getClass());

   protected final TransactionLocal entitiesFifoMap = new TransactionLocal();       // map of fifo ordered entity lists
   protected final TransactionLocal entitiesSetMap = new TransactionLocal();        // map of entity sets for quick lookups
   /**
    * associate entity with transaction
    */
   public void associate(
         Transaction tx,
         EntityEnterpriseContext entity)
      throws RollbackException, SystemException
   {
      List entityFifoList = (List)entitiesFifoMap.get(tx);
      Set entitySet;
         if (entityFifoList == null)
         {
            entityFifoList = new ArrayList();
            entitySet = new HashSet();
            entitiesFifoMap.set(tx, entityFifoList);
            entitiesSetMap.set(tx, entitySet);
            tx.registerSynchronization(new GlobalTxEntityMapSynchronize(tx));
         }
         else
         {
            entitySet = (Set)entitiesSetMap.get(tx);
         }
      //Release lock on txToEntitiesFifoMap to avoid waiting for possibly long scans of
      //entityFifoList.

      //if all tx only modify one or two entities, the two synchs here will be
      //slower than doing all work in one synch block on txToEntityMap.
      //However, I (david jencks) think the risk of waiting for scans of long
      //entityLists is greater than the risk of waiting for 2 synchs.

      // On the other hand, I (steve coy) have found that these long scans are a
      // significant performance bottleneck.
      // The overhead of maintaining the parallel HashSet MORE than makes up for
      // itself.

      //There should be only one thread associated with this tx at a time.
      //Therefore we should not need to synchronize on entityFifoList to ensure exclusive
      //access.  entityFifoList is correct since it was obtained in a synch block.

      if (!entitySet.contains(entity))
      {
            entityFifoList.add(entity);
            entitySet.add(entity);
      }
   }

   /**
    * sync all EntityEnterpriseContext that are involved (and changed)
    * within a transaction.
    */
   public void synchronizeEntities(Transaction tx)
   {
      ArrayList entities = (ArrayList)entitiesFifoMap.get(tx);

      //There should be only one thread associated with this tx at a time.
      //Therefore we should not need to synchronize on entityFifoList to ensure exclusive
      //access.  entityFifoList is correct since it was obtained in a synch block.

      // if there are there no entities associated with this tx we are done
      if (entities == null)
      {
         return;
      }
      Set entitySet = (Set)entitiesSetMap.get(tx);
      // This is an independent point of entry. We need to make sure the
      // thread is associated with the right context class loader
      final Thread currentThread = Thread.currentThread();
      ClassLoader oldCl = currentThread.getContextClassLoader();

      EntityEnterpriseContext ctx = null;
      try
      {
         for (int i = 0; i < entities.size(); i++)
         {
            // any one can mark the tx rollback at any time so check
            // before continuing to the next store
            if (tx.getStatus() == Status.STATUS_MARKED_ROLLBACK)
            {
               // nothing else to do here
               return;
            }

            // read-only entities will never get into this list.
            ctx = (EntityEnterpriseContext)entities.get(i);

            // only synchronize if the id is not null.  A null id means
            // that the entity has been removed.
            if(ctx.getId() != null)
            {
               EntityContainer container = (EntityContainer)ctx.getContainer();

               // set the context class loader before calling the store method
               currentThread.setContextClassLoader(
                  container.getClassLoader());

               // store it
               container.storeEntity(ctx);
            }
         }
      }
      catch (Exception causeByException)
      {
         // EJB 1.1 section 12.3.2 and EJB 2 section 18.3.3
         // exception during store must log exception, mark tx for
         // rollback and throw a TransactionRolledback[Local]Exception
         // if using caller's transaction.  All of this is handled by
         // the AbstractTxInterceptor and LogInterceptor.
         //
         // All we need to do here is mark the transacction for rollback
         // and rethrow the causeByException.  The caller will handle logging
         // and wraping with TransactionRolledback[Local]Exception.
         try
         {
            tx.setRollbackOnly();
         }
         catch(Exception e)
         {
            log.warn("Exception while trying to rollback tx: " + tx, e);
         }

         // Rethrow cause by exception
         if(causeByException instanceof EJBException)
         {
            throw (EJBException)causeByException;
         }
         throw new EJBException("Exception in store of entity:" +
                                ((ctx == null)? "<null>": ctx.getId().toString()), causeByException);
      }
      finally
      {
         currentThread.setContextClassLoader(oldCl);
         entities.clear();
         entitySet.clear();
      }
   }




   /**
    * Store changed entities to resource manager in this Synchronization
    */
   private class GlobalTxEntityMapSynchronize implements Synchronization
   {
      Transaction tx;

      public GlobalTxEntityMapSynchronize(Transaction tx)
      {
         this.tx = tx;
      }

      // Synchronization implementation -----------------------------

      public void beforeCompletion()
      {
         if (log.isTraceEnabled())
         {
            log.trace("beforeCompletion called for tx " + tx);
         }

         // let the runtime exceptions fall out, so the committer can determine
         // the root cause of a rollback
         synchronizeEntities(tx);
      }

      public void afterCompletion(int status)
      {
         //no-op
      }
   }
}
