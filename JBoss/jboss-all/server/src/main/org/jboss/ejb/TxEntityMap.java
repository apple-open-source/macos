/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb;

import java.util.HashMap;
import javax.transaction.Transaction;
import javax.transaction.RollbackException;
import javax.transaction.SystemException;
import javax.transaction.Synchronization;

/**
 * This class provides a way to find out what entities of a certain type that are contained in
 * within a transaction.  It is attached to a specific instance of a container.
 *<no longer - global only holds possibly dirty> This class interfaces with the static GlobalTxEntityMap.  EntitySynchronizationInterceptor
 * registers tx/entity pairs through this class.
 * Used in EntitySynchronizationInterceptor.
 * 
 * @author <a href="bill@burkecentral.com">Bill Burke</a>
 * @version $Revision: 1.9 $
 *
 * Revisions:
 *
 * <p><b>Revisions:</b><br>
 * <p><b>2001/08/06: billb</b>
 * <ol>
 *   <li>Got rid of disassociate and added a javax.transaction.Synchronization.  The sync will clean up the map now.
 *   <li>This class now interacts with GlobalTxEntityMap available.
 * </ol>
 */
public class TxEntityMap
{
   protected HashMap m_map = new HashMap();
    
   /**
    * associate entity with transaction
    */
   public synchronized void associate(Transaction tx,
                                      EntityEnterpriseContext entity) throws RollbackException, SystemException
   {
      HashMap entityMap = (HashMap)m_map.get(tx);
      if (entityMap == null)
      {
         entityMap = new HashMap();
         m_map.put(tx, entityMap);
         tx.registerSynchronization(new TxEntityMapCleanup(this, tx));
      }
      //EntityContainer.getGlobalTxEntityMap().associate(tx, entity);
      entityMap.put(entity.getCacheKey(), entity);
   }

   public synchronized EntityEnterpriseContext getCtx(Transaction tx,
                                                      Object key)
   {
      HashMap entityMap = (HashMap)m_map.get(tx);
      if (entityMap == null) return null;
      return (EntityEnterpriseContext)entityMap.get(key);
   }

   /**
    * Cleanup tx/entity map on tx commit/rollback
    */
   private class TxEntityMapCleanup implements Synchronization
   {
      TxEntityMap map;
      Transaction tx;

      public TxEntityMapCleanup(TxEntityMap map,
                                Transaction tx)
      {
         this.map = map;
         this.tx = tx;
      }

      // Synchronization implementation -----------------------------
  
      public void beforeCompletion()
      {
         /* complete */
      }
  
      public void afterCompletion(int status)
      {
         synchronized(map)
         {
            HashMap entityMap = (HashMap)m_map.remove(tx);
            if (entityMap != null)
            {
               entityMap.clear();
            }
         }
      }
   }
}
