/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.jdbc.bridge;

import java.lang.reflect.Field;

import java.util.Map;

import javax.ejb.EJBException;

import org.jboss.deployment.DeploymentException;
import org.jboss.ejb.EntityEnterpriseContext;

import org.jboss.ejb.plugins.cmp.jdbc.JDBCContext;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCStoreManager;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCCMPFieldMetaData;


/**
 * JDBCCMP1xFieldBridge is a concrete implementation of JDBCCMPFieldBridge for 
 * CMP version 1.x. Getting and setting of instance fields set the 
 * corresponding field in bean instance.  Dirty checking is performed by 
 * storing the current value in the entity persistence context when ever
 * setClean is called, and comparing current value to the original value.
 *
 * Life-cycle:
 *      Tied to the EntityBridge.
 *
 * Multiplicity:   
 *      One for each entity bean cmp field.       
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.13 $
 */                            
public class JDBCCMP1xFieldBridge extends JDBCAbstractCMPFieldBridge {
   private Field field;
    
   public JDBCCMP1xFieldBridge(
         JDBCStoreManager manager,
         JDBCCMPFieldMetaData metadata) throws DeploymentException {

      super(manager, metadata);

      try {
         field = manager.getMetaData().getEntityClass().getField(
               getFieldName());
      } catch(NoSuchFieldException e) {
         // Non recoverable internal exception
         throw new DeploymentException("No field named '" + getFieldName() + 
               "' found in entity class.");
      }
   }

   public Object getInstanceValue(EntityEnterpriseContext ctx) {
      FieldState fieldState = getFieldState(ctx);
      if(!fieldState.isLoaded) {
         throw new EJBException("CMP 1.1 field not loaded: " + getFieldName());
      }

      try {
         return field.get(ctx.getInstance());
      } catch(Exception e) {
         // Non recoverable internal exception
         throw new EJBException("Internal error getting instance field " +
               getFieldName(), e);
      }
   }
   
   public void setInstanceValue(EntityEnterpriseContext ctx, Object value) {
      try {
         field.set(ctx.getInstance(), value);

         FieldState fieldState = getFieldState(ctx);
         fieldState.isLoaded = true;
      } catch(Exception e) {
         // Non recoverable internal exception
         throw new EJBException("Internal error setting instance field " + 
               getFieldName(), e);
      }
   }

   public boolean isLoaded(EntityEnterpriseContext ctx) {
      return getFieldState(ctx).isLoaded;
   }
   
   /**
    * Has the value of this field changes since the last time clean was called.
    */
   public boolean isDirty(EntityEnterpriseContext ctx) {
      // read only and primary key fields are never dirty
      if(isReadOnly() || isPrimaryKeyMember()) {
         return false; 
      }

      // has the value changes since setClean
      return changed(getInstanceValue(ctx), getFieldState(ctx).originalValue);
   }
   
   /**
    * Mark this field as clean.
    * Saves the current state in context, so it can be compared when 
    * isDirty is called.
    */
   public void setClean(EntityEnterpriseContext ctx) {
      FieldState fieldState = getFieldState(ctx);
      fieldState.originalValue = getInstanceValue(ctx);

      // update last read time
      if(isReadOnly()) {
         fieldState.lastRead = System.currentTimeMillis();
      }
   }

   public boolean isReadTimedOut(EntityEnterpriseContext ctx) {
      // if we are read/write then we are always timed out
      if(!isReadOnly()) {
         return true;
      }

      // if read-time-out is -1 then we never time out.
      if(getReadTimeOut() == -1) {
         return false;
      }

      long readInterval = System.currentTimeMillis() - 
            getFieldState(ctx).lastRead; 
      return readInterval >= getReadTimeOut();
   }
   
   public void resetPersistenceContext(EntityEnterpriseContext ctx) {
      if(isReadTimedOut(ctx)) {
         JDBCContext jdbcCtx = (JDBCContext)ctx.getPersistenceContext();
         jdbcCtx.put(this, new FieldState());
      }
   }

   private FieldState getFieldState(EntityEnterpriseContext ctx) {
      JDBCContext jdbcCtx = (JDBCContext)ctx.getPersistenceContext();
      FieldState fieldState = (FieldState)jdbcCtx.get(this);
      if(fieldState == null) {
         fieldState = new FieldState();
         jdbcCtx.put(this, fieldState);
      }
      return fieldState;
   }

   private static class FieldState {
      boolean isLoaded = false;
      Object originalValue;
      long lastRead = -1;
   }      
}
