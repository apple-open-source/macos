/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.cmp.jdbc.bridge;

import java.lang.reflect.Field;

import javax.ejb.EJBException;
import javax.ejb.EJBLocalObject;

import org.jboss.deployment.DeploymentException;
import org.jboss.ejb.EntityEnterpriseContext;

import org.jboss.ejb.plugins.cmp.jdbc.JDBCContext;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCStoreManager;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCType;
import org.jboss.ejb.plugins.cmp.jdbc.CMPFieldStateFactory;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCCMPFieldMetaData;

/**
 * JDBCCMP2xFieldBridge is a concrete implementation of JDBCCMPFieldBridge for
 * CMP version 2.x. Instance data is stored in the entity persistence context.
 * Whenever a field is changed it is compared to the current value and sets
 * a dirty flag if the value has changed.
 *
 * Life-cycle:
 *      Tied to the EntityBridge.
 *
 * Multiplicity:
 *      One for each entity bean cmp field.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 * @version $Revision: 1.13.4.33 $
 */
public class JDBCCMP2xFieldBridge extends JDBCAbstractCMPFieldBridge
{
   /** column name (used only at deployment time to check whether fields mapped to the same column) */
   private final String columnName;

   /** CMP field this foreign key field is mapped to */
   private final JDBCCMP2xFieldBridge cmpFieldIAmMappedTo;

   /** this is used for foreign key fields mapped to CMP fields (check ChainLink) */
   private ChainLink cmrChainLink;

   // Constructors

   public JDBCCMP2xFieldBridge(JDBCStoreManager manager,
                               JDBCCMPFieldMetaData metadata)
      throws DeploymentException
   {
      super(manager, metadata);
      cmpFieldIAmMappedTo = null;
      columnName = metadata.getColumnName();
   }

   public JDBCCMP2xFieldBridge(JDBCStoreManager manager,
                               JDBCCMPFieldMetaData metadata,
                               CMPFieldStateFactory stateFactory,
                               boolean checkDirtyAfterGet)
      throws DeploymentException
   {
      this(manager, metadata);
      this.stateFactory = stateFactory;
      this.checkDirtyAfterGet = checkDirtyAfterGet;
   }

   public JDBCCMP2xFieldBridge(JDBCCMP2xFieldBridge cmpField,
                               CMPFieldStateFactory stateFactory,
                               boolean checkDirtyAfterGet)
      throws DeploymentException
   {
      this(
         cmpField.getManager(),
         cmpField.getFieldName(),
         cmpField.getFieldType(),
         cmpField.getJDBCType(),
         cmpField.isReadOnly(),               // should always be false?
         cmpField.getReadTimeOut(),
         cmpField.getPrimaryKeyClass(),
         cmpField.getPrimaryKeyField(),
         cmpField,
         null,                                // it should not be a foreign key
         cmpField.getColumnName()
      );
      this.stateFactory = stateFactory;
      this.checkDirtyAfterGet = checkDirtyAfterGet;
   }

   /**
    * This constructor creates a foreign key field.
    */
   public JDBCCMP2xFieldBridge(JDBCStoreManager manager,
                               JDBCCMPFieldMetaData metadata,
                               JDBCType jdbcType)
      throws DeploymentException
   {
      super(manager, metadata, jdbcType);
      cmpFieldIAmMappedTo = null;
      columnName = metadata.getColumnName();
   }

   /**
    * This constructor is used to create a foreign key field instance that is
    * a part of primary key field. See JDBCCMRFieldBridge.
    */
   public JDBCCMP2xFieldBridge(JDBCStoreManager manager,
                               String fieldName,
                               Class fieldType,
                               JDBCType jdbcType,
                               boolean readOnly,
                               long readTimeOut,
                               Class primaryKeyClass,
                               Field primaryKeyField,
                               JDBCCMP2xFieldBridge cmpFieldIAmMappedTo,
                               JDBCCMRFieldBridge myCMRField,
                               String columnName)
      throws DeploymentException
   {
      super(
         manager,
         fieldName,
         fieldType,
         jdbcType,
         readOnly,
         readTimeOut,
         primaryKeyClass,
         primaryKeyField,
         cmpFieldIAmMappedTo.getFieldIndex(),
         cmpFieldIAmMappedTo.getTableIndex(),
         cmpFieldIAmMappedTo.checkDirtyAfterGet,
         cmpFieldIAmMappedTo.stateFactory
      );
      this.cmpFieldIAmMappedTo = cmpFieldIAmMappedTo;
      if(myCMRField != null)
      {
         cmrChainLink = new CMRChainLink(myCMRField);
         cmpFieldIAmMappedTo.addCMRChainLink(cmrChainLink);
      }
      this.columnName = columnName;
   }

   // Public

   public JDBCCMP2xFieldBridge getCmpFieldIAmMappedTo()
   {
      return cmpFieldIAmMappedTo;
   }

   public ChainLink getCmrChainLink()
   {
      return cmrChainLink;
   }

   public boolean isFKFieldMappedToCMPField()
   {
      return cmpFieldIAmMappedTo != null && this.cmrChainLink != null;
   }

   public String getColumnName()
   {
      return columnName;
   }

   // JDBCFieldBridge implementation

   public Object getInstanceValue(EntityEnterpriseContext ctx)
   {
      // notify optimistic lock
      FieldState fieldState = getFieldState(ctx);
      if(!fieldState.isLoaded())
      {
         manager.loadField(this, ctx);
         if(!fieldState.isLoaded())
            throw new EJBException("Could not load field value: " + getFieldName());
      }

      return fieldState.getValue();
   }

   public void setInstanceValue(EntityEnterpriseContext ctx, Object value)
   {
      FieldState fieldState = getFieldState(ctx);

      // update current value
      if(cmpFieldIAmMappedTo != null && cmpFieldIAmMappedTo.isPrimaryKeyMember())
      {
         if(value != null && fieldState.isValueChanged(value))
         {
            throw new IllegalStateException(
               "New value [" + value + "] of a foreign key field "
               + getFieldName()
               + " changed the value of a primary key field "
               + cmpFieldIAmMappedTo.getFieldName()
               + "[" + fieldState.value + "]");
         }
         // else value is not null and equals to the previous value -> nothing to do
      }
      else
      {
         if(cmrChainLink != null
            && fieldState.isLoaded()
            && fieldState.isValueChanged(value))
         {
            cmrChainLink.execute(ctx, fieldState, value);
         }

         fieldState.setValue(value);
      }

      // we are loading the field right now so it isLoaded
      fieldState.setLoaded();
   }

   public void lockInstanceValue(EntityEnterpriseContext ctx)
   {
      getFieldState(ctx).lockValue();
   }

   public boolean isLoaded(EntityEnterpriseContext ctx)
   {
      return getFieldState(ctx).isLoaded();
   }

   /**
    * Has the value of this field changes since the last time clean was called.
    */
   public boolean isDirty(EntityEnterpriseContext ctx)
   {
      return !primaryKeyMember
         && !readOnly
         && getFieldState(ctx).isDirty();
   }

   /**
    * Mark this field as clean. Saves the current state in context, so it
    * can be compared when isDirty is called.
    */
   public void setClean(EntityEnterpriseContext ctx)
   {
      FieldState fieldState = getFieldState(ctx);
      fieldState.setClean();

      // update last read time
      if(readOnly && readTimeOut != -1)
         fieldState.lastRead = System.currentTimeMillis();
   }

   public void resetPersistenceContext(EntityEnterpriseContext ctx)
   {
      if(isReadTimedOut(ctx))
      {
         JDBCContext jdbcCtx = (JDBCContext)ctx.getPersistenceContext();
         FieldState fieldState = (FieldState)jdbcCtx.getFieldState(jdbcContextIndex);
         if(fieldState != null)
            fieldState.reset();
      }
   }

   public boolean isReadTimedOut(EntityEnterpriseContext ctx)
   {
      // if we are read/write then we are always timed out
      if(!readOnly)
         return true;

      // if read-time-out is -1 then we never time out.
      if(readTimeOut == -1)
         return false;

      long readInterval = System.currentTimeMillis() - getFieldState(ctx).lastRead;
      return readInterval >= readTimeOut;
   }

   public Object getLockedValue(EntityEnterpriseContext ctx)
   {
      return getFieldState(ctx).getLockedValue();
   }

   public void updateState(EntityEnterpriseContext ctx, Object value)
   {
      getFieldState(ctx).updateState(value);
   }

   protected void setDirtyAfterGet(EntityEnterpriseContext ctx)
   {
      getFieldState(ctx).setCheckDirty();
   }

   // Private

   private void addCMRChainLink(ChainLink nextCMRChainLink)
   {
      if(cmrChainLink == null)
      {
         cmrChainLink = new DummyChainLink();
      }
      cmrChainLink.setNextLink(nextCMRChainLink);
   }

   private FieldState getFieldState(EntityEnterpriseContext ctx)
   {
      JDBCContext jdbcCtx = (JDBCContext)ctx.getPersistenceContext();
      FieldState fieldState = (FieldState)jdbcCtx.getFieldState(jdbcContextIndex);
      if(fieldState == null)
      {
         fieldState = new FieldState(jdbcCtx);
         jdbcCtx.setFieldState(jdbcContextIndex, fieldState);
      }
      return fieldState;
   }

   // Inner

   private class FieldState
   {
      /** entity's state this field state belongs to */
      private JDBCEntityBridge.EntityState entityState;
      /** current field value */
      private Object value;
      /** previous field state. NOTE: it might not be the same as previous field value */
      private Object state;
      /** locked field value */
      private Object lockedValue;
      /** last time the field was read */
      private long lastRead = -1;

      public FieldState(JDBCContext jdbcCtx)
      {
         this.entityState = jdbcCtx.getEntityState();
      }

      /**
       * Reads current field value.
       * @return current field value.
       */
      public Object getValue()
      {
         //if(checkDirtyAfterGet)
         //   setCheckDirty();
         return value;
      }

      /**
       * Sets new field value and sets the flag that setter was called on the field
       * @param newValue  new field value.
       */
      public void setValue(Object newValue)
      {
         this.value = newValue;
         setCheckDirty();
      }

      private void setCheckDirty()
      {
         entityState.setCheckDirty(tableIndex);
      }

      /**
       * @return true if the field is loaded.
       */
      public boolean isLoaded()
      {
         return entityState.isLoaded(tableIndex);
      }

      /**
       * Marks the field as loaded.
       */
      public void setLoaded()
      {
         entityState.setLoaded(tableIndex);
      }

      /**
       * @return true if the field is dirty.
       */
      public boolean isDirty()
      {
         return isLoaded() && !stateFactory.isStateValid(state, value);
      }

      /**
       * Compares current value to a new value. Note, it does not compare
       * field states, just values.
       * @param newValue  new field value
       * @return true if field values are not equal.
       */
      public boolean isValueChanged(Object newValue)
      {
         return value == null ? newValue != null : !value.equals(newValue);
      }

      /**
       * Resets masks and updates the state.
       */
      public void setClean()
      {
         entityState.setClean(tableIndex);
         updateState(value);
      }

      /**
       * Updates the state to some specific value that might be different from the current
       * field's value. This trick is needed for foreign key fields because they can be
       * changed while not being loaded. When the owning CMR field is loaded this method is
       * called with the loaded from the database value. Thus, we have correct state and locked value.
       * @param value  the value loaded from the database.
       */
      private void updateState(Object value)
      {
         state = stateFactory.getFieldState(value);
         lockedValue = value;
      }

      /**
       * Resets everything.
       */
      public void reset()
      {
         value = null;
         state = null;
         lastRead = -1;
         entityState.resetFlags(tableIndex);
      }

      public void lockValue()
      {
         if(entityState.lockValue(tableIndex))
         {
            //log.debug("locking> " + fieldName + "=" + value);
            lockedValue = value;
         }
      }

      public Object getLockedValue()
      {
         return lockedValue;
      }
   }

   /**
    * Represents a link in the chain. The execute method will doExecute each link
    * in the chain except for the link (originator) execute() was called on.
    */
   private abstract static class ChainLink
   {
      private ChainLink nextLink;

      public ChainLink()
      {
         nextLink = this;
      }

      public void setNextLink(ChainLink nextLink)
      {
         nextLink.nextLink = this.nextLink;
         this.nextLink = nextLink;
      }

      public ChainLink getNextLink()
      {
         return nextLink;
      }

      public void execute(EntityEnterpriseContext ctx,
                          FieldState fieldState,
                          Object newValue)
      {
         nextLink.doExecute(this, ctx, fieldState, newValue);
      }

      protected abstract void doExecute(ChainLink originator,
                                        EntityEnterpriseContext ctx,
                                        FieldState fieldState,
                                        Object newValue);
   }

   /**
    * This chain link contains a CMR field a foreign key of which is mapped to a CMP field.
    */
   private static class CMRChainLink
      extends ChainLink
   {
      private final JDBCCMRFieldBridge cmrField;

      public CMRChainLink(JDBCCMRFieldBridge cmrField)
      {
         this.cmrField = cmrField;
      }

      /**
       * Going down the chain current related id is calculated and stored in oldRelatedId.
       * When the next link is originator, the flow is going backward:
       * - field state is updated with new vaue;
       * - new related id is calculated;
       * - old relationship is destroyed (if there is one);
       * - new relationship is established (if it is valid).
       *
       * @param originator  ChainLink that started execution.
       * @param ctx  EnterpriseEntityContext of the entity.
       * @param fieldState  field's state.
       * @param newValue  new field value.
       */
      public void doExecute(ChainLink originator,
                            EntityEnterpriseContext ctx,
                            FieldState fieldState,
                            Object newValue)
      {
         // get old related id
         Object oldRelatedId = cmrField.getRelatedIdFromContext(ctx);

         // invoke down the cmrChain
         if(originator != getNextLink())
         {
            getNextLink().doExecute(originator, ctx, fieldState, newValue);
         }

         // update field state
         fieldState.setValue(newValue);

         // get new related id
         Object newRelatedId = cmrField.getRelatedIdFromContext(ctx);

         // destroy old relationship
         if(oldRelatedId != null)
            destroyRelations(oldRelatedId, ctx);

         // establish new relationship
         if(newRelatedId != null)
            createRelations(newRelatedId, ctx);
      }

      private void createRelations(Object newRelatedId, EntityEnterpriseContext ctx)
      {
         try
         {
            EJBLocalObject relatedEntity = cmrField.getRelatedEntityByFK(newRelatedId);
            if(relatedEntity != null)
            {
               cmrField.createRelationLinks(ctx, newRelatedId, false);
            }
            else
            {
               // set foreign key to a new value
               cmrField.setForeignKey(ctx, newRelatedId);
               // put calculated relatedId to the waiting list
               cmrField.getRelatedCMRField().addRelatedPKWaitingForMyPK(newRelatedId, ctx.getId());
            }
         }
         catch(Exception e)
         {
            // no such object
         }
      }

      private void destroyRelations(Object oldRelatedId, EntityEnterpriseContext ctx)
      {
         cmrField.getRelatedCMRField().removeRelatedPKWaitingForMyPK(oldRelatedId, ctx.getId());
         try
         {
            EJBLocalObject relatedEntity = cmrField.getRelatedEntityByFK(oldRelatedId);
            if(relatedEntity != null)
            {
               cmrField.destroyRelationLinks(ctx, oldRelatedId, true, false);
            }
         }
         catch(Exception e)
         {
            // no such object
         }
      }
   }

   private static class DummyChainLink
      extends ChainLink
   {
      public void doExecute(ChainLink originator,
                            EntityEnterpriseContext ctx,
                            FieldState fieldState,
                            Object newValue)
      {
         // invoke down the cmrChain
         if(originator != getNextLink())
         {
            getNextLink().doExecute(originator, ctx, fieldState, newValue);
         }
         // update field state
         fieldState.setValue(newValue);
      }
   }
}
