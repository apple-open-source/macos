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

import org.jboss.tm.TransactionLocal;

/**
 * This class provides a way to find out what entities of a certain type that are contained in
 * within a transaction.  It is attached to a specific instance of a container.
 *<no longer - global only holds possibly dirty> This class interfaces with the static GlobalTxEntityMap.  EntitySynchronizationInterceptor
 * registers tx/entity pairs through this class.
 * Used in EntitySynchronizationInterceptor.
 *
 * @author <a href="bill@burkecentral.com">Bill Burke</a>
 * @version $Revision: 1.9.4.4 $
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
   protected TransactionLocal m_map = new TransactionLocal();

   /**
    * associate entity with transaction
    */
   public void associate(Transaction tx, EntityEnterpriseContext entity)
      throws RollbackException, SystemException
   {
      HashMap entityMap = (HashMap) m_map.get(tx);
      if(entityMap == null)
      {
         entityMap = new HashMap();
         m_map.set(tx, entityMap);
      }
      //EntityContainer.getGlobalTxEntityMap().associate(tx, entity);
      entityMap.put(entity.getCacheKey(), entity);
   }

   public EntityEnterpriseContext getCtx(Transaction tx, Object key)
   {
      HashMap entityMap = (HashMap) m_map.get(tx);
      if(entityMap == null) return null;
      return (EntityEnterpriseContext) entityMap.get(key);
   }

   public EntityEnterpriseContext getCtx(Object key)
   {
      HashMap entityMap = (HashMap) m_map.get();
      if(entityMap == null) return null;
      return (EntityEnterpriseContext) entityMap.get(key);
   }
}
