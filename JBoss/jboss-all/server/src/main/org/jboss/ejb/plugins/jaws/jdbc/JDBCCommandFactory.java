/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.jaws.jdbc;



import java.lang.ref.ReferenceQueue;
import java.lang.ref.SoftReference;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Map;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.transaction.Transaction;
import javax.transaction.TransactionManager;
import org.jboss.deployment.DeploymentException;
import org.jboss.ejb.EntityContainer;
import org.jboss.ejb.plugins.jaws.JPMActivateEntityCommand;
import org.jboss.ejb.plugins.jaws.JPMCommandFactory;
import org.jboss.ejb.plugins.jaws.JPMCreateEntityCommand;
import org.jboss.ejb.plugins.jaws.JPMDestroyCommand;
import org.jboss.ejb.plugins.jaws.JPMFindEntitiesCommand;
import org.jboss.ejb.plugins.jaws.JPMFindEntityCommand;
import org.jboss.ejb.plugins.jaws.JPMInitCommand;
import org.jboss.ejb.plugins.jaws.JPMLoadEntityCommand;
import org.jboss.ejb.plugins.jaws.JPMPassivateEntityCommand;
import org.jboss.ejb.plugins.jaws.JPMRemoveEntityCommand;
import org.jboss.ejb.plugins.jaws.JPMStartCommand;
import org.jboss.ejb.plugins.jaws.JPMStopCommand;
import org.jboss.ejb.plugins.jaws.JPMStoreEntityCommand;
import org.jboss.ejb.plugins.jaws.metadata.FinderMetaData;
import org.jboss.ejb.plugins.jaws.metadata.JawsApplicationMetaData;
import org.jboss.ejb.plugins.jaws.metadata.JawsEntityMetaData;
import org.jboss.ejb.plugins.jaws.metadata.JawsXmlFileLoader;
import org.jboss.logging.Logger;
import org.jboss.metadata.ApplicationMetaData;
import org.jboss.util.TimerQueue;
import org.jboss.util.TimerTask;

/**
 * Command factory for the JAWS JDBC layer. This class is primarily responsible
 * for creating instances of the JDBC implementations for the various JPM
 * commands so that the JAWSPersistenceManager (actually an persistence store)
 * can delegate to them in a decoupled manner.
 * <p>This class also acts as the manager for the read-ahead buffer added in
 * version 2.3/2.4. In order to manage this buffer, it must register itself
 * with any transaction that is active when a finder is called so that the
 * data that was read ahead can be discarded before completion of the
 * transaction. The read ahead buffer is managed using Soft references, with
 * a ReferenceQueue being used to tell when the VM has garbage collected an
 * object so that we can keep the hashtables clean.
 *
 * @author <a href="mailto:justin@j-m-f.demon.co.uk">Justin Forder</a>
 * @author <a href="danch@nvisia.com">danch (Dan Christopherson)</a>
 * @author <a href="bill@burkecentral.com">Bill Burke</a>
 * @version $Revision: 1.21 $
 *
 *   <p><b>Revisions:</b>
 *
 *   <p><b>20010621 Bill Burke:</b>
 *   <ul>
 *   <li>createDefinedFinderCommand creates different objects
 *    based on the read-head flag of the FinderMetaData.
 *   </ul>
 *
 *   <p><b>20010621 danch:</b>
 *   <ul>
 *   <li>extended Bill's change to work on other finder types;
 *    removed stale todos.
 *   </ul>
 *
 *   <p><b>20010812 vincent.harcq@hubmethods.com:</b>
 *   <ul>
 *   <li> Get Rid of debug flag, use log4j instead
 *   </ul>
 *
 */
public class JDBCCommandFactory implements JPMCommandFactory
{
   // Attributes ----------------------------------------------------
   
   private EntityContainer container;
   private JawsEntityMetaData metadata;
   private final Logger log = Logger.getLogger(this.getClass());
   
   /** Timer queue used to time polls on the preloadRefQueue on all JAWS
    *  handled entities
    */
   private static TimerQueue softRefHandler;

   /**
    * The variable <code>reqQue</code> is given to the soft ref que timer, and saved
    * here so it may be unregistered.
    */
   private PreloadRefQueueHandlerTask reqQue;
   
   /** Timer queue used to get references to preload data who've been GC'ed */
   private ReferenceQueue preloadRefQueue = new ReferenceQueue();
   
   /** a map of data preloaded within some transaction for some entity. This map
    *  is keyed by Transaction and the data are hashmaps with key = entityKey and
    *  data = Object[] containing the entity data.  */
   private Map preloadedData = new HashMap();
   /** A map of data preloaded without a transaction context. key=entityKey,
    *  data = Object[] containing entity data
    */
   private Map nonTransactionalPreloadData = new HashMap();
   
   /** a Transaction manager so that we can link preloaded data to a transaction */
   private TransactionManager tm;
   
   // These support singletons (within the scope of this factory)
   private JDBCBeanExistsCommand beanExistsCommand;
   private JPMFindEntitiesCommand findEntitiesCommand;
   
   //static initializer to kick off our softRefhandler
   static
   {
      softRefHandler = new TimerQueue("JAWS Preload reference handler");
      softRefHandler.start();
   }
   
   // Constructors --------------------------------------------------
   
   public JDBCCommandFactory(EntityContainer container)
   throws Exception
   {
      this.container = container;
      
      String ejbName = container.getBeanMetaData().getEjbName();
      ApplicationMetaData amd = container.getBeanMetaData().getApplicationMetaData();
      JawsApplicationMetaData jamd = (JawsApplicationMetaData)amd.getPluginData("JAWS");
      
      if (jamd == null)
      {
         // we are the first cmp entity to need jaws. Load jaws.xml for the whole application
         JawsXmlFileLoader jfl = new JawsXmlFileLoader(amd, container.getClassLoader(), container.getLocalClassLoader());
         jamd = jfl.load();
         amd.addPluginData("JAWS", jamd);
      }
      
      metadata = jamd.getBeanByEjbName(ejbName);
      if (metadata == null)
      {
         throw new DeploymentException("No metadata found for bean " + ejbName);
      }
      
      tm = (TransactionManager) container.getTransactionManager();

      reqQue = new PreloadRefQueueHandlerTask(preloadRefQueue,
         preloadedData, nonTransactionalPreloadData);
      softRefHandler.schedule(reqQue);
   }

   //lifecycle -- just need this one for now.

   public void destroy()
   {
      reqQue.cancel();
      reqQue = null;
   }
   
   // Public --------------------------------------------------------
   
   public EntityContainer getContainer()
   {
      return container;
   }
   
   public JawsEntityMetaData getMetaData()
   {
      return metadata;
   }
   
   // Additional Command creation
   
   /**
    * Singleton: multiple callers get references to the
    * same command instance.
    */
   public JDBCBeanExistsCommand createBeanExistsCommand()
   {
      if (beanExistsCommand == null)
      {
         beanExistsCommand = new JDBCBeanExistsCommand(this);
      }
      
      return beanExistsCommand;
   }
   
   public JPMFindEntitiesCommand createFindAllCommand(FinderMetaData f)
   {
      if (f.hasReadAhead())
      {
         return new JDBCPreloadFinderCommand(this, new JDBCFindAllCommand(this, f));
      }
      else
      {
         return new JDBCFindAllCommand(this, f);
      }
   }
   
   public JPMFindEntitiesCommand createDefinedFinderCommand(FinderMetaData f)
   {
      if (f.hasReadAhead())
      {
         return new JDBCPreloadFinderCommand(this, f);
      }
      else
      {
         return new JDBCDefinedFinderCommand(this, f);
      }
   }
   
   public JPMFindEntitiesCommand createFindByCommand(Method finderMethod, FinderMetaData f)
   throws IllegalArgumentException
   {
      if (f.hasReadAhead())
      {
         return new JDBCPreloadFinderCommand(this, new JDBCFindByCommand(this, finderMethod, f));
      }
      else
      {
         return new JDBCFindByCommand(this, finderMethod, f);
      }
   }
   
   // JPMCommandFactory implementation ------------------------------
   
   // lifecycle commands
   
   public JPMInitCommand createInitCommand()
   {
      return new JDBCInitCommand(this);
   }
   
   public JPMStartCommand createStartCommand()
   {
      return new JDBCStartCommand(this);
   }
   
   public JPMStopCommand createStopCommand()
   {
      return new JDBCStopCommand(this);
   }
   
   public JPMDestroyCommand createDestroyCommand()
   {
      return new JDBCDestroyCommand(this);
   }
   
   // entity persistence-related commands
   
   public JPMFindEntityCommand createFindEntityCommand()
   {
      return new JDBCFindEntityCommand(this);
   }
   
   /**
    * Singleton: multiple callers get references to the
    * same command instance.
    */
   public JPMFindEntitiesCommand createFindEntitiesCommand()
   {
      if (findEntitiesCommand == null)
      {
         findEntitiesCommand = new JDBCFindEntitiesCommand(this);
      }
      
      return findEntitiesCommand;
   }
   
   public JPMCreateEntityCommand createCreateEntityCommand()
   {
      return new JDBCCreateEntityCommand(this);
   }
   
   public JPMRemoveEntityCommand createRemoveEntityCommand()
   {
      return new JDBCRemoveEntityCommand(this);
   }
   
   public JPMLoadEntityCommand createLoadEntityCommand()
   {
      return new JDBCLoadEntityCommand(this);
   }
   
   public JPMStoreEntityCommand createStoreEntityCommand()
   {
      return new JDBCStoreEntityCommand(this);
   }
   
   // entity activation and passivation commands
   
   public JPMActivateEntityCommand createActivateEntityCommand()
   {
      return new JDBCActivateEntityCommand(this);
   }
   
   public JPMPassivateEntityCommand createPassivateEntityCommand()
   {
      return new JDBCPassivateEntityCommand(this);
   }
   
   
   /** Add preloaded data for an entity within the scope of a transaction */
   /*package*/ void addPreloadData(Object entityKey, Object[] entityData)
   {
      Transaction trans = null;
      try
      {
         trans = tm.getTransaction();
      } catch (javax.transaction.SystemException sysE)
      {
         log.warn("System exception getting transaction for preload - can't preload data for "+entityKey, sysE);
         return;
      }
      //log.debug("PRELOAD: adding preload for "+entityKey+" in transaction "+(trans != null ? trans.toString() : "NONE")+" entityData="+entityData);
      
      if (trans != null)
      {
         synchronized (preloadedData)
         {
            Map entitiesInTransaction = (Map)preloadedData.get(trans);
            if (entitiesInTransaction == null)
            {
               try
               {
                  trans.registerSynchronization(new PreloadClearSynch(trans));
               } catch (javax.transaction.SystemException se)
               {
                  log.warn("System exception getting transaction for preload - can't get preloaded data for "+entityKey, se);
                  return;
               } catch (javax.transaction.RollbackException re)
               {
                  log.warn("Rollback exception getting transaction for preload - can't get preloaded data for "+entityKey, re);
                  return;
               }
               entitiesInTransaction = new HashMap();
               preloadedData.put(trans, entitiesInTransaction);
            }
            PreloadData preloadData = new PreloadData(trans, entityKey, entityData, preloadRefQueue);
            entitiesInTransaction.put(entityKey, preloadData);
         }
      } else
      {
         synchronized (nonTransactionalPreloadData)
         {
            PreloadData preloadData = new PreloadData(null, entityKey, entityData, preloadRefQueue);
            nonTransactionalPreloadData.put(entityKey, preloadData);
         }
      }
   }
   
   /** get data that we might have preloaded for an entity in a transaction -
    *  may return null!
    */
   /*package*/ Object[] getPreloadData(Object entityKey)
   {
      Transaction trans = null;
      try
      {
         trans = tm.getTransaction();
      } catch (javax.transaction.SystemException sysE)
      {
         log.warn("System exception getting transaction for preload - not preloading "+entityKey, sysE);
         return null;
      }
      
      Object[] result = null;
      PreloadData preloadData = null;
      if (trans != null)
      {
         Map entitiesInTransaction = null;
         // Do we really need this to be syncrhonized? What is the effect of
         //    another thread trying to modify this map? It won't be to remove
         //    our transaction (we're in it here!, trying to call a business
         //    method), and who cares if another is added/removed?
         //         synchronized (preloadedData) {
         entitiesInTransaction = (Map)preloadedData.get(trans);
         //         }
         if (entitiesInTransaction != null)
         {
            synchronized (entitiesInTransaction)
            {
               preloadData = (PreloadData)entitiesInTransaction.get(entityKey);
               entitiesInTransaction.remove(entityKey);
            }
         }
      } else
      {
         synchronized (nonTransactionalPreloadData)
         {
            preloadData = (PreloadData)nonTransactionalPreloadData.get(entityKey);
            nonTransactionalPreloadData.remove(entityKey);
         }
      }
      if (preloadData != null)
      {
         result = preloadData.getData();
      } /*else {
         log.debug("PRELOAD: preloadData == null for "+entityKey);
      }
if (result == null)
   log.debug("PRELOAD: returning null as preload for "+entityKey);
       */
      return result;
   }
   
   /** clear out any data we have preloaded for any entity in this transaction */
   /*package*/ void clearPreloadForTrans(Transaction trans)
   {
      //log.debug("PRELOAD: clearing preload for transaction "+trans.toString());
      synchronized (preloadedData)
      {
         preloadedData.remove(trans);
      }
   }
   
   
   // Private -------------------------------------------------------
   
   /** Static class that handles our reference queue. It is a static class
    to avoid a strong reference to the container instance variable as this
    prevents GC of the container.
    */
   private static class PreloadRefQueueHandlerTask extends TimerTask
   {
      ReferenceQueue preloadRefQueue;
      Map preloadedData;
      Map nonTransactionalPreloadData;

      PreloadRefQueueHandlerTask(ReferenceQueue preloadRefQueue,
         Map preloadedData, Map nonTransactionalPreloadData)
      {
         super(50);
         this.preloadRefQueue = preloadRefQueue;
         this.preloadedData = preloadedData;
         this.nonTransactionalPreloadData = nonTransactionalPreloadData;
      }

      public void execute() throws Exception
      {
         PreloadData preloadData = (PreloadData)preloadRefQueue.poll();
         int handled = 0;
         while (preloadData != null && handled < 10)
         {
            if (preloadData.getTransaction() != null)
            {
               Map entitiesInTransaction = null;
               // Do we really need this to be syncrhonized? What is the effect of
               //    another thread trying to modify this map? It won't be to remove
               //    our transaction (we're in it here!, trying to call a business
               //    method), and who cares if another is added/removed?
               //         synchronized (preloadedData) {
               entitiesInTransaction = (Map)preloadedData.get(preloadData.getTransaction());
               //         }
               if (entitiesInTransaction != null)
               {
                  synchronized (entitiesInTransaction)
                  {
                     entitiesInTransaction.remove(preloadData.getKey());
                  }
               }
            } else
            {
               synchronized (nonTransactionalPreloadData)
               {
                  nonTransactionalPreloadData.remove(preloadData.getKey());
               }
            }
            preloadData.empty();
            handled++;
            
            preloadData = (PreloadData)preloadRefQueue.poll();
         }
      }
   }
   
   /** Inner class used in the preload Data hashmaps so that we can wrap a
    *  SoftReference around the data and still have enough information to remove
    *  the reference from the appropriate hashMap.
    */
   private class PreloadData extends SoftReference
   {
      private Object key;
      private Transaction trans;
      
      PreloadData(Transaction trans, Object key, Object[] data, ReferenceQueue queue)
      {
         super(data, queue);
         this.trans = trans;
         this.key = key;
      }
      
      Transaction getTransaction()
      {
         return trans;
      }
      Object getKey()
      {
         return key;
      }
      Object[] getData()
      {
         return (Object[])get();
      }
      
      /** Named empty to not collide with superclass clear */
      public void empty()
      {
         key = null;
         trans = null;
      }
   }
   
   private class PreloadClearSynch implements javax.transaction.Synchronization
   {
      private Transaction forTrans;
      public PreloadClearSynch(Transaction forTrans)
      {
         this.forTrans = forTrans;
      }
      public void afterCompletion(int p0)
      {
         clearPreloadForTrans(forTrans);
      }
      public void beforeCompletion()
      {
         //no-op
      }
   }
}
