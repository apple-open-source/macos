/**
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc;

import javax.ejb.EJBException;

import org.jboss.ejb.Container;
import org.jboss.ejb.EntityContainer;
import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.invocation.Invocation;
import org.jboss.ejb.plugins.AbstractInterceptor;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.CMRMessage;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMRFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.CMRInvocation;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCRelationMetaData;
import org.jboss.logging.Logger;

/**
 *
 * The role of this interceptor relationship messages from a related CMR field
 * and invoke the specified message on this container's cmr field of the
 * relationship.  This interceptor also manages the relation table data.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.13.2.6 $
 */
public final class JDBCRelationInterceptor extends AbstractInterceptor
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
      if(container != null)
      {
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
      if(!(mi instanceof CMRInvocation))
         return getNext().invoke(mi);

      CMRMessage relationshipMessage = ((CMRInvocation)mi).getCmrMessage();
      if(relationshipMessage == null)
      {
         // Not a relationship message. Invoke down the chain
         return getNext().invoke(mi);
      }

      // We are going to work with the context a lot
      EntityEnterpriseContext ctx = (EntityEnterpriseContext)mi.getEnterpriseContext();
      JDBCCMRFieldBridge cmrField = (JDBCCMRFieldBridge)mi.getArguments()[0];

      if(CMRMessage.GET_RELATED_ID == relationshipMessage)
      {
         // call getRelateId
         if(log.isTraceEnabled())
         {
            log.trace("Getting related id: field=" + cmrField.getFieldName() + " id=" + ctx.getId());
         }
         return cmrField.getRelatedId(ctx);

      }
      else if(CMRMessage.ADD_RELATION == relationshipMessage)
      {
         // call addRelation
         Object relatedId = mi.getArguments()[1];
         if(log.isTraceEnabled())
         {
            log.trace("Add relation: field=" + cmrField.getFieldName() +
               " id=" + ctx.getId() +
               " relatedId=" + relatedId);
         }
         cmrField.addRelation(ctx, relatedId);

         // WARN: do not check
         //if(cmrField.isCollectionValued() && cmrField.getRelatedCMRField().isCollectionValued())
         // it could be 1:1 with relation table mapping
         if(!cmrField.hasForeignKey() && !cmrField.getRelatedCMRField().hasForeignKey())
         {
            RelationData relationData = getRelationData(cmrField);
            relationData.addRelation(
               cmrField,
               ctx.getId(),
               cmrField.getRelatedCMRField(),
               relatedId);
         }
         return null;

      }
      else if(CMRMessage.REMOVE_RELATION == relationshipMessage)
      {
         // call removeRelation
         Object relatedId = mi.getArguments()[1];
         if(log.isTraceEnabled())
         {
            log.trace("Remove relation: field=" + cmrField.getFieldName() +
               " id=" + ctx.getId() +
               " relatedId=" + relatedId);
         }
         cmrField.removeRelation(ctx, relatedId);

         // WARN: do not check
         //if(cmrField.isCollectionValued() && cmrField.getRelatedCMRField().isCollectionValued())
         // it could be 1:1 with relation table mapping
         if(!cmrField.hasForeignKey() && !cmrField.getRelatedCMRField().hasForeignKey())
         {
            RelationData relationData = getRelationData(cmrField);
            relationData.removeRelation(
               cmrField,
               ctx.getId(),
               cmrField.getRelatedCMRField(),
               relatedId);
         }

         return null;
      }
      else if(CMRMessage.SCHEDULE_FOR_CASCADE_DELETE == relationshipMessage)
      {
         cmrField.getEntity().scheduleForCascadeDelete(ctx);
         return null;
      }
      else if(CMRMessage.SCHEDULE_FOR_BATCH_CASCADE_DELETE == relationshipMessage)
      {
         cmrField.getEntity().scheduleForBatchCascadeDelete(ctx);
         return null;
      }
      else
      {
         // this should not be possible we are using a type safe enum
         throw new EJBException("Unknown cmp2.0-relationship-message=" +
            relationshipMessage);
      }
   }

   private static RelationData getRelationData(JDBCCMRFieldBridge cmrField)
   {
      JDBCStoreManager manager = cmrField.getJDBCStoreManager();
      JDBCRelationMetaData relationMetaData = cmrField.getMetaData().getRelationMetaData();
      RelationData relationData = (RelationData)manager.getApplicationTxData(relationMetaData);

      if(relationData == null)
      {
         relationData = new RelationData(cmrField, cmrField.getRelatedCMRField());
         manager.putApplicationTxData(relationMetaData, relationData);
      }
      return relationData;
   }

   // Private  ----------------------------------------------------
}

