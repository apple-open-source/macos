/**
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc;

import java.lang.reflect.Method;
import javax.ejb.EJBException;
import org.jboss.ejb.Container;
import org.jboss.ejb.EntityContainer;
import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.invocation.Invocation;
import org.jboss.ejb.plugins.AbstractInterceptor;
import org.jboss.ejb.plugins.CMPPersistenceManager;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.CMRMessage;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMRFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCRelationMetaData;
import org.jboss.logging.Logger;

/**
 *
 * The role of this interceptor relationship messages from a related CMR field
 * and invoke the specified message on this container's cmr field of the
 * relationship.  This interceptor also manages the relation table data.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.13.2.1 $
 */
public class JDBCRelationInterceptor extends AbstractInterceptor
{
   // Attributes ----------------------------------------------------
   
   /**
    *  The container of this interceptor.
    */
   private EntityContainer container;

   /**
    * The log.
    */ 
   private Logger log;
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   // Public --------------------------------------------------------
   public void setContainer(Container container)
   {
      this.container = (EntityContainer)container;

      JDBCStoreManager manager = null;
      if( container != null )
      {
         try
         {
            EntityContainer entityContainer = (EntityContainer)container;
            CMPPersistenceManager cmpManager = 
                 (CMPPersistenceManager)entityContainer.getPersistenceManager();
            manager = (JDBCStoreManager) cmpManager.getPersistenceStore();
         }
         catch(ClassCastException e)
         {
            throw new EJBException("JDBCRealtionInteceptor can only be used " +
                  "JDBCStoreManager", e);
         }

         log = Logger.getLogger(
               this.getClass().getName() +
               "." + 
               container.getBeanMetaData().getEjbName());
      }
   }
   
   public Container getContainer()
   {
      return container;
   }
   
   // Interceptor implementation --------------------------------------
   
   public Object invoke(Invocation mi) throws Exception
   {
      // We are going to work with the context a lot
      EntityEnterpriseContext ctx =
            (EntityEnterpriseContext)mi.getEnterpriseContext();

      CMRMessage relationshipMessage = 
            (CMRMessage)mi.getValue(CMRMessage.CMR_MESSAGE_KEY);

      if(relationshipMessage == null) {
         // Not a relationship message. Invoke down the chain
         return getNext().invoke(mi);
      } else if(CMRMessage.GET_RELATED_ID == relationshipMessage)
      {
         // call getRelateId
         JDBCCMRFieldBridge cmrField =
               (JDBCCMRFieldBridge)mi.getArguments()[0];
         if(log.isTraceEnabled()) {
            log.trace("Getting related id: field=" + cmrField.getFieldName() +
                  " id=" + ctx.getId());
         }
         return cmrField.getRelatedId(ctx);
         
      } else if(CMRMessage.ADD_RELATION == relationshipMessage)
      {
         
         // call addRelation
         JDBCCMRFieldBridge cmrField =
               (JDBCCMRFieldBridge)mi.getArguments()[0];
         
         Object relatedId = mi.getArguments()[1];
         if(log.isTraceEnabled()) {
            log.trace("Add relation: field=" + cmrField.getFieldName() +
                  " id=" + ctx.getId() +
                  " relatedId=" + relatedId);
         }
         cmrField.addRelation(ctx, relatedId);
         
         RelationData relationData = getRelationData(cmrField);
         relationData.addRelation(
               cmrField,
               ctx.getId(),
               cmrField.getRelatedCMRField(),
               relatedId);

         return null;
         
      } else if(CMRMessage.REMOVE_RELATION == relationshipMessage)
      {
         
         // call removeRelation
         JDBCCMRFieldBridge cmrField = 
               (JDBCCMRFieldBridge)mi.getArguments()[0];
         
         Object relatedId = mi.getArguments()[1];
         if(log.isTraceEnabled()) {
            log.trace("Remove relation: field=" + cmrField.getFieldName() +
                  " id=" + ctx.getId() +
                  " relatedId=" + relatedId);
         }
         cmrField.removeRelation(ctx, relatedId);
         
         RelationData relationData = getRelationData(cmrField);
         relationData.removeRelation(
               cmrField,
               ctx.getId(),
               cmrField.getRelatedCMRField(),
               relatedId);

         return null;
      } else if(CMRMessage.INIT_RELATED_CTX == relationshipMessage) {
         return null;
      } else
      {
         // this should not be possible we are using a type safe enum
         throw new EJBException("Unknown cmp2.0-relationship-message=" +
               relationshipMessage);
      }
   }
   
   private RelationData getRelationData(JDBCCMRFieldBridge cmrField)
   {
      JDBCStoreManager manager = cmrField.getJDBCStoreManager();
      JDBCRelationMetaData relationMetaData = 
            cmrField.getMetaData().getRelationMetaData();

      
      RelationData relationData = 
            (RelationData)manager.getApplicationTxData(relationMetaData);

      if(relationData == null)
      {
         relationData = new RelationData(
               cmrField, cmrField.getRelatedCMRField());
         manager.putApplicationTxData(relationMetaData, relationData);
      }
      return relationData;
   }

   // Private  ----------------------------------------------------
}

