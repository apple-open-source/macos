/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.cmp.jdbc.bridge;

import java.lang.reflect.Field;
import java.util.List;
import java.util.ArrayList;
import java.util.Iterator;

import javax.ejb.EJBException;
import javax.ejb.EJBLocalObject;

import org.jboss.deployment.DeploymentException;
import org.jboss.ejb.EntityEnterpriseContext;

import org.jboss.ejb.plugins.cmp.jdbc.JDBCContext;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCStoreManager;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCType;
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
 * @version $Revision: 1.13.4.19 $
 */
public class JDBCCMP2xFieldBridge extends JDBCAbstractCMPFieldBridge
{
   // Attributes for a foreign key field mapped to a CMP field
   /** CMR field this foreign key field is a part of */
   private final JDBCCMRFieldBridge myCMRField;
   /** CMP field this foreign key field is mapped to */
   private final JDBCCMP2xFieldBridge cmpFieldIAmMappedTo;

   // Attributes for CMP field that have a foreign key field[s] mapped to it
   /** the list of foreign key fields mapped to this CMP field */
   private final List fkFieldsMappedToMe = new ArrayList();

   // Constructors

   public JDBCCMP2xFieldBridge(JDBCStoreManager manager,
                               JDBCCMPFieldMetaData metadata)
      throws DeploymentException
   {
      super(manager, metadata);
      myCMRField = null;
      cmpFieldIAmMappedTo = null;
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
      myCMRField = null;
      cmpFieldIAmMappedTo = null;
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
                               JDBCCMRFieldBridge myCMRField,
                               JDBCCMP2xFieldBridge cmpFieldIAmMappedTo)
      throws DeploymentException
   {
      super(manager,
         fieldName,
         fieldType,
         jdbcType,
         readOnly,
         readTimeOut,
         false,
         primaryKeyClass,
         primaryKeyField,
         false,
         false
      );
      this.myCMRField = myCMRField;
      this.cmpFieldIAmMappedTo = cmpFieldIAmMappedTo;
      cmpFieldIAmMappedTo.addFkFieldMappedToMe(this);
   }

   // Public

   public JDBCCMP2xFieldBridge getCmpFieldIAmMappedTo()
   {
      return cmpFieldIAmMappedTo;
   }

   public boolean isFKFieldMappedToCMPField()
   {
      return cmpFieldIAmMappedTo != null;
   }

   public FieldState getFieldState(EntityEnterpriseContext ctx)
   {
      JDBCContext jdbcCtx = (JDBCContext)ctx.getPersistenceContext();
      FieldState fieldState = (FieldState)jdbcCtx.get(this);
      if(fieldState == null)
      {
         fieldState = new FieldState();
         jdbcCtx.put(this, fieldState);
      }
      return fieldState;
   }

   public void setInstanceValue(EntityEnterpriseContext ctx,
                                Object value,
                                JDBCCMP2xFieldBridge dontUpdateThisFKField)
   {
      setInstanceValue(ctx, value, dontUpdateThisFKField, true);
   }

   // JDBCFieldBridge implementation

   public Object getInstanceValue(EntityEnterpriseContext ctx)
   {
      // notify optimistic lock
      getManager().fieldStateEventCallback(ctx, CMPMessage.ACCESSED, this);

      FieldState fieldState = getFieldState(ctx);
      if(!fieldState.isLoaded)
      {
         if(cmpFieldIAmMappedTo != null)
         {
            return cmpFieldIAmMappedTo.getInstanceValue(ctx);
         }
         else
         {
            getManager().loadField(this, ctx);
            if(!fieldState.isLoaded)
               throw new EJBException("Could not load field value: " + getFieldName());
         }
      }

      return fieldState.value;
   }

   public void setInstanceValue(EntityEnterpriseContext ctx, Object value)
   {
      setInstanceValue(ctx, value, null, !isPrimaryKeyMember());
   }

   public boolean isLoaded(EntityEnterpriseContext ctx)
   {
      return getFieldState(ctx).isLoaded;
   }

   /**
    * Has the value of this field changes since the last time clean was called.
    */
   public boolean isDirty(EntityEnterpriseContext ctx)
   {
      // read only and primary key fields are never dirty
      if(isReadOnly() || isPrimaryKeyMember())
         return false;
      return getFieldState(ctx).isDirty;
   }

   /**
    * Mark this field as clean. Saves the current state in context, so it
    * can be compared when isDirty is called.
    */
   public void setClean(EntityEnterpriseContext ctx)
   {
      FieldState fieldState = getFieldState(ctx);
      fieldState.isDirty = false;

      // update last read time
      if(isReadOnly())
         fieldState.lastRead = System.currentTimeMillis();
   }

   public void resetPersistenceContext(EntityEnterpriseContext ctx)
   {
      if(isReadTimedOut(ctx))
      {
         JDBCContext jdbcCtx = (JDBCContext)ctx.getPersistenceContext();
         jdbcCtx.put(this, new FieldState());
      }
   }

   public boolean isReadTimedOut(EntityEnterpriseContext ctx)
   {
      // if we are read/write then we are always timed out
      if(!isReadOnly())
         return true;

      // if read-time-out is -1 then we never time out.
      if(getReadTimeOut() == -1)
         return false;

      long readInterval = System.currentTimeMillis() - getFieldState(ctx).lastRead;
      return readInterval >= getReadTimeOut();
   }

   // Private

   private void setInstanceValue(EntityEnterpriseContext ctx,
                                 Object value,
                                 JDBCCMP2xFieldBridge dontUpdateThisFKField,
                                 boolean updateFKFieldsMappedToMe)
   {
      FieldState fieldState = getFieldState(ctx);

      // determine if this change should mark the field dirty
      if(getManager().getEntityBridge().getVersionField() == this)
      {
         // this is the version field so do not treat as dirty
         fieldState.isDirty = false;
      }
      else if(isFKFieldMappedToCMPField())
      {
         // this is a FK field mapped to a PK field so cannot be dirty
         fieldState.isDirty = false;
      }
      else if(dontUpdateThisFKField != null)
      {
         // this is a CMP field that is updated by the change of a FK field mapped to it
         // it is dirty only if the fk field is already loaded
         fieldState.isDirty = dontUpdateThisFKField.isLoaded(ctx);
      }
      else
      {
         // OK, we really are dirty
         fieldState.isDirty = true;
      }

      // notify optimistic lock, but only if the bean is created
      if(ctx.getId() != null && fieldState.isLoaded())
         getManager().fieldStateEventCallback(ctx, CMPMessage.CHANGED, this);

      boolean changed = changed(fieldState.value, value);

      // update current value
      fieldState.value = value;

      // update cmp field this fk field is mapped to before this fk is marked as loaded
      if(cmpFieldIAmMappedTo != null
         && !cmpFieldIAmMappedTo.isPrimaryKeyMember()
         && changed)
         cmpFieldIAmMappedTo.setInstanceValue(ctx, value, this);

      // update foreign key fields mapped to this CMP field and
      // mark their CMR fields as not loaded
      if(updateFKFieldsMappedToMe
         && !fkFieldsMappedToMe.isEmpty()
         && ctx.isValid()
         && changed
         && fieldState.isLoaded)
         updateFKFieldsMappedToMe(ctx, value, dontUpdateThisFKField);

      // we are loading the field right now so it isLoaded
      fieldState.isLoaded = true;

   }

   private void addFkFieldMappedToMe(JDBCCMP2xFieldBridge fkFieldMappedToMe)
   {
      fkFieldsMappedToMe.add(fkFieldMappedToMe);
   }

   /**
    * Updates foreign key fields mapped to this CMP field.
    * @param ctx - entity's context;
    * @param newFieldValue - new newFieldValue;
    * @param excludeFKField - foreign key field to exclude from list of fields to update.
    */
   private void updateFKFieldsMappedToMe(EntityEnterpriseContext ctx,
                                         Object newFieldValue,
                                         JDBCCMP2xFieldBridge excludeFKField)
   {
      for(Iterator fkIter = fkFieldsMappedToMe.iterator(); fkIter.hasNext();)
      {
         JDBCCMP2xFieldBridge fkFieldMappedToMe = (JDBCCMP2xFieldBridge)fkIter.next();
         if(fkFieldMappedToMe == excludeFKField)
            continue;

         Object curRelatedId = fkFieldMappedToMe.myCMRField.getRelatedIdFromContextFK(ctx);

         // ATTENTION HERE
         // Update fkFieldMappedToMe to a new newFieldValue before destroying relationship.
         // Because foreign key can be complex and destroying relationship will set
         // all foreign key fields to NULL. To work around this, update this
         // foreign key field preserving other foreign key fields and get
         // id newFieldValue for potentially related entity. Then destroy relationship
         // NULLifying all the foreign key fields. And finally if there is related
         // entity with calculated id, set foreign key fields to related values.
         fkFieldMappedToMe.setInstanceValue(ctx, newFieldValue);
         Object newRelatedId = fkFieldMappedToMe.myCMRField.getRelatedIdFromContextCMP(ctx);

         // destroy old relationship
         if(curRelatedId != null)
         {
            fkFieldMappedToMe.myCMRField.getRelatedCMRField().
               removeRelatedPKWaitingForMyPK(curRelatedId, ctx.getId());

            try
            {
               EJBLocalObject relatedEntity = fkFieldMappedToMe.myCMRField.getRelatedEntityByFK(curRelatedId, ctx);
               if(relatedEntity != null)
                  fkFieldMappedToMe.myCMRField.destroyRelationLinks(ctx, curRelatedId);
            }
            catch(Exception e)
            {
               // no such object
            }
         }

         // establish new relationship
         if(newRelatedId != null)
         {
            try
            {
               EJBLocalObject relatedEntity = fkFieldMappedToMe.myCMRField.getRelatedEntityByFK(newRelatedId, ctx);
               if(relatedEntity != null)
                  fkFieldMappedToMe.myCMRField.createRelationLinks(ctx, newRelatedId);
               else
               {
                  // set foreign key to a new value
                  fkFieldMappedToMe.myCMRField.setForeignKey(ctx, newRelatedId);
                  // put calculated relatedId to the waiting list
                  fkFieldMappedToMe.myCMRField.getRelatedCMRField().
                     addRelatedPKWaitingForMyPK(newRelatedId, ctx.getId());
               }
            }
            catch(Exception e)
            {
               // no such object
            }
         }
      }
   }

   // Inner ------------------------------------------------
   public static class FieldState
   {
      // Attributes ----------------------------------------
      private Object value;
      private boolean isLoaded = false;
      private boolean isDirty = false;
      private long lastRead = -1;

      // Public --------------------------------------------
      public Object getValue()
      {
         return value;
      }

      public boolean isLoaded()
      {
         return isLoaded;
      }

      public boolean isDirty()
      {
         return isDirty;
      }
   }
}
