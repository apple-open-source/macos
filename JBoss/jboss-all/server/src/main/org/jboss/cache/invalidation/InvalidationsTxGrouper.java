/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.cache.invalidation;

import javax.transaction.Transaction;
import org.jboss.logging.Logger;
import java.util.HashMap;
import javax.transaction.Synchronization;
import org.jboss.cache.invalidation.InvalidationGroup;
import java.io.Serializable;
import java.util.HashSet;
import javax.transaction.Status;
import java.util.Iterator;

/**
 * Utility class that can be used to group invalidations in a set of
 * BatchInvalidations structure and only commit them alltogether at
 * transaction commit-time.
 * The invalidations are grouped (in this order):
 * - by transaction
 * - by InvalidationManager instance
 * - by InvalidationGroup
 *
 * This object will manage the transaction registering by itself if not
 * already done.
 * Thus, once a transaction commits, it will prepare a set of BatchInvalidation
 * collections (one for each InvalidationManager involved): on BI instance
 * for each InvalidationGroup. Then it will call the IM.batchInvalidation
 * method.
 *
 * @see InvalidationManagerMBean
 * @see BatchInvalidation
 * @see InvalidationsTxGrouper.InvalidationSynchronization
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>26 septembre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class InvalidationsTxGrouper
{
   
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   // Static --------------------------------------------------------
   
   protected static HashMap synchronizations = new HashMap();
   protected static Logger log = Logger.getLogger(InvalidationsTxGrouper.class);

   // Constructors --------------------------------------------------
   
   // Public --------------------------------------------------------
   
   public static void registerInvalidationSynchronization(Transaction tx, InvalidationGroup group, Serializable key) throws Exception
   {
      InvalidatorSynchronization synch = null;
      synchronized(synchronizations)
      {
         synch = (InvalidatorSynchronization)synchronizations.get(tx);
         if (synch == null)
         {
            synch = new InvalidatorSynchronization(tx);
            tx.registerSynchronization(synch);
         }
      }
      synch.addInvalidation(group, key);
   }

   // Z implementation ----------------------------------------------
   
   // Y overrides ---------------------------------------------------
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------

}

    class InvalidatorSynchronization
      implements Synchronization
   {
      /**
       *  The transaction we follow.
       */
      protected Transaction tx;
  
      /**
       *  The context we manage.
       */
      protected HashMap ids = new HashMap();
  
      /**
       *  Create a new isynchronization instance.
       */
      InvalidatorSynchronization(Transaction tx)
      {
         this.tx = tx;
      }

      public void addInvalidation(InvalidationGroup group, Serializable key)
      {
         InvalidationManagerMBean im = group.getInvalidationManager ();

         // the grouping is (in order): by InvalidationManager, by InvalidationGroup
         //
         HashMap relatedInvalidationMgr = (HashMap)ids.get(im);         
         if (relatedInvalidationMgr == null)
         {
            synchronized (ids)
            {
               relatedInvalidationMgr = (HashMap)ids.get(im); // to avoid race conditions
               if (relatedInvalidationMgr == null)
               {
                  relatedInvalidationMgr = new HashMap ();
                  ids.put (im, relatedInvalidationMgr);
               }               
            }
         }
         
         HashSet relatedInvalidations = (HashSet)relatedInvalidationMgr.get(group);         
         if (relatedInvalidations == null)
         {
            synchronized (relatedInvalidationMgr)
            {
               relatedInvalidations = (HashSet)relatedInvalidationMgr.get(group); // to avoid race conditions
               if (relatedInvalidations == null)
               {
                  relatedInvalidations = new HashSet ();
                  relatedInvalidationMgr.put (group, relatedInvalidations);
               }               
            }
         }
         
         relatedInvalidations.add(key);
      }
  
      // Synchronization implementation -----------------------------
  
      
      public void beforeCompletion()
      {
         // This is an independent point of entry. We need to make sure the
         // thread is associated with the right context class loader
         //
         ClassLoader oldCl = Thread.currentThread().getContextClassLoader();
         Thread.currentThread().setContextClassLoader(this.getClass().getClassLoader());
         
         try
         {
            int status = Status.STATUS_ROLLEDBACK;
            try { 
               status = tx.getStatus(); 
            }
            catch (javax.transaction.SystemException se)
            {
               InvalidationsTxGrouper.log.error("Failed to get transaction status: ", se);
            }
            if (status != Status.STATUS_ROLLEDBACK
                || status != Status.STATUS_MARKED_ROLLBACK)
            {
               try
               {
                  sendBatchInvalidations();
               }
               catch (Exception ex)
               {
                  InvalidationsTxGrouper.log.warn("Failed sending invalidations messages", ex);
               }
            }
            synchronized (InvalidationsTxGrouper.synchronizations)
            {
               InvalidationsTxGrouper.synchronizations.remove(tx);
            }
         }
         finally
         {
            Thread.currentThread().setContextClassLoader(oldCl);
         }
      }
      
  
      public void afterCompletion(int status)
      {
         // complete
      }
      
      protected void sendBatchInvalidations()
      {
         // we iterate over all InvalidationManager involved
         //
         Iterator imIter = ids.keySet ().iterator ();
         while (imIter.hasNext ())
         {
            InvalidationManagerMBean im = (InvalidationManagerMBean)imIter.next ();
            
            // get associated groups
            //            
            HashMap relatedInvalidationMgr = (HashMap)ids.get(im);     
            
            BatchInvalidation[] bomb = new BatchInvalidation[relatedInvalidationMgr.size ()];
            
            Iterator groupsIter = relatedInvalidationMgr.keySet ().iterator ();
            int i=0;
            while (groupsIter.hasNext ())
            {
               InvalidationGroup group = (InvalidationGroup)groupsIter.next ();
               HashSet sourceIds = (HashSet)relatedInvalidationMgr.get (group);
               
               Serializable[] ids = new Serializable[sourceIds.size ()];
               sourceIds.toArray (ids);
               BatchInvalidation batch = new BatchInvalidation (ids, group.getGroupName ());
               
               bomb[i] = batch;
               
               i++;
            }
            
            // do the batch-invalidation for this IM
            //
            im.batchInvalidate (bomb);
            
         }
         
         // Help the GC to remove this big structure
         //
         this.ids = null;
         
      }
      
   }
   