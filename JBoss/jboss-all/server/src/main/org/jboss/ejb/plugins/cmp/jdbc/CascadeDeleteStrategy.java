/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc;

import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMRFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCEntityBridge;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCRelationshipRoleMetaData;
import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.logging.Logger;
import org.jboss.deployment.DeploymentException;

import javax.ejb.EJBLocalObject;
import javax.ejb.RemoveException;
import java.util.Set;
import java.util.List;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Map;
import java.util.HashMap;
import java.sql.Connection;
import java.sql.PreparedStatement;

/**
 *
 * @author <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
 */
public abstract class CascadeDeleteStrategy
{
   /**
    * No cascade-delete strategy.
    */
   public static final class NoneCascadeDeleteStrategy
      extends CascadeDeleteStrategy
   {
      public NoneCascadeDeleteStrategy(JDBCCMRFieldBridge cmrField)
      {
         super(cmrField);
      }

      public boolean removeFromRelations(EntityEnterpriseContext ctx, Object[] oldRelationsRef)
      {
         boolean removed = false;
         Object value = cmrField.getInstanceValue(ctx);
         if(cmrField.isCollectionValued())
         {
            Set c = (Set)value;
            if(!c.isEmpty())
            {
               removed = true;
               cmrField.setInstanceValue(ctx, null);
            }
         }
         else
         {
            if(value != null)
            {
               removed = true;
               cmrField.setInstanceValue(ctx, null);
            }
         }
         return removed;
      }

      public void cascadeDelete(EntityEnterpriseContext ctx, List oldValues)
         throws RemoveException
      {
         boolean debug = log.isDebugEnabled();
         for(int i = 0; i < oldValues.size(); ++i)
         {
            EJBLocalObject oldValue = (EJBLocalObject)oldValues.get(i);
            if(relatedManager.uncheduledCascadeDelete(oldValue))
            {
               if(debug)
                  log.debug("Removing " + oldValue);

               oldValue.remove();
            }
            else
            {
               if(debug)
                  log.debug(oldValue + " already removed");
            }
         }
      }
   }

   /**
    * Specification compliant cascade-delete strategy, i.e. one DELETE per child
    */
   public static final class DefaultCascadeDeleteStrategy
      extends CascadeDeleteStrategy
   {
      public DefaultCascadeDeleteStrategy(JDBCCMRFieldBridge cmrField)
      {
         super(cmrField);
      }

      public boolean removeFromRelations(EntityEnterpriseContext ctx, Object[] oldRelationsRef)
      {
         boolean removed = false;
         Object value = cmrField.getInstanceValue(ctx);
         if(cmrField.isCollectionValued())
         {
            Set c = (Set)value;
            if(!c.isEmpty())
            {
               removed = true;
               cmrField.scheduleChildrenForCascadeDelete(ctx);
               scheduleCascadeDelete(oldRelationsRef, new ArrayList(c));
               cmrField.setInstanceValue(ctx, null);
            }
         }
         else
         {
            if(value != null)
            {
               removed = true;
               cmrField.scheduleChildrenForCascadeDelete(ctx);
               scheduleCascadeDelete(oldRelationsRef, Collections.singletonList(value));
               cmrField.setInstanceValue(ctx, null);
            }
         }
         return removed;
      }

      public void cascadeDelete(EntityEnterpriseContext ctx, List oldValues) throws RemoveException
      {
         boolean debug = log.isDebugEnabled();
         for(int i = 0; i < oldValues.size(); ++i)
         {
            EJBLocalObject oldValue = (EJBLocalObject)oldValues.get(i);
            if(relatedManager.uncheduledCascadeDelete(oldValue))
            {
               if(debug)
                  log.debug("Removing " + oldValue);

               oldValue.remove();
            }
            else
            {
               if(debug)
                  log.debug(oldValue + " already removed");
            }
         }
      }
   }

   /**
    * Batch cascade-delete strategy. Deletes children with one statement of the form
    * DELETE FROM RELATED_TABLE WHERE FOREIGN_KEY = ?
    */
   public static final class BatchCascadeDeleteStrategy
      extends CascadeDeleteStrategy
   {
      private final String batchCascadeDeleteSql;

      public BatchCascadeDeleteStrategy(JDBCCMRFieldBridge cmrField)
         throws DeploymentException
      {
         super(cmrField);

         if(cmrField.hasForeignKey())
         {
            throw new DeploymentException(
               "Batch cascade-delete was setup for the role with a foreign key: relationship "
               + cmrField.getMetaData().getRelationMetaData().getRelationName()
               + ", role " + cmrField.getMetaData().getRelationshipRoleName()
               + ". Batch cascade-delete supported only for roles with no foreign keys."
            );
         }

         StringBuffer buf = new StringBuffer(100);
         buf.append("DELETE FROM ")
            .append(cmrField.getRelatedJDBCEntity().getTableName())
            .append(" WHERE ");
         SQLUtil.getWhereClause(cmrField.getRelatedCMRField().getForeignKeyFields(), buf);
         batchCascadeDeleteSql = buf.toString();

         log.debug(
            cmrField.getMetaData().getRelationMetaData().getRelationName() + " batch cascade delete SQL: "
            + batchCascadeDeleteSql
         );
      }

      public boolean removeFromRelations(EntityEnterpriseContext ctx, Object[] oldRelationsRef)
      {
         boolean removed = false;
         Object value = cmrField.getInstanceValue(ctx);
         if(cmrField.isCollectionValued())
         {
            Set c = (Set)value;
            if(!c.isEmpty())
            {
               removed = true;
               cmrField.scheduleChildrenForBatchCascadeDelete(ctx);
               scheduleCascadeDelete(oldRelationsRef, new ArrayList(c));
            }
         }
         else
         {
            if(value != null)
            {
               removed = true;
               cmrField.scheduleChildrenForBatchCascadeDelete(ctx);
               scheduleCascadeDelete(oldRelationsRef, Collections.singletonList(value));
            }
         }

         return removed;
      }

      public void cascadeDelete(EntityEnterpriseContext ctx, List oldValues) throws RemoveException
      {
         boolean didDelete = false;
         boolean debug = log.isDebugEnabled();
         for(int i = 0; i < oldValues.size(); ++i)
         {
            EJBLocalObject oldValue = (EJBLocalObject)oldValues.get(i);
            if(relatedManager.uncheduledCascadeDelete(oldValue))
            {
               if(debug)
                  log.debug("Removing " + oldValue);
               oldValue.remove();
               didDelete = true;
            }
            else
            {
               if(debug)
                  log.debug(oldValue + " already removed");
            }
         }

         if(didDelete)
         {
            executeDeleteSQL(batchCascadeDeleteSql, ctx.getId());
         }
      }
   }

   public static CascadeDeleteStrategy getCascadeDeleteStrategy(JDBCCMRFieldBridge cmrField)
      throws DeploymentException
   {
      CascadeDeleteStrategy result;
      JDBCRelationshipRoleMetaData relatedRole = cmrField.getMetaData().getRelatedRole();
      if(relatedRole.isBatchCascadeDelete())
      {
         result = new BatchCascadeDeleteStrategy(cmrField);
      }
      else if(relatedRole.isCascadeDelete())
      {
         result = new DefaultCascadeDeleteStrategy(cmrField);
      }
      else
      {
         result = new NoneCascadeDeleteStrategy(cmrField);
      }
      return result;
   }

   protected final JDBCCMRFieldBridge cmrField;
   protected final JDBCEntityBridge entity;
   protected final JDBCStoreManager relatedManager;
   protected final Logger log;

   public CascadeDeleteStrategy(JDBCCMRFieldBridge cmrField)
   {
      this.cmrField = cmrField;
      entity = cmrField.getEntity();
      relatedManager = cmrField.getRelatedManager();
      log = Logger.getLogger(getClass().getName() + "." + cmrField.getEntity().getEntityName());
   }

   public abstract boolean removeFromRelations(EntityEnterpriseContext ctx, Object[] oldRelationsRef);

   public abstract void cascadeDelete(EntityEnterpriseContext ctx, List oldValues) throws RemoveException;

   protected void scheduleCascadeDelete(Object[] oldRelationsRef, List values)
   {
      Map oldRelations = (Map)oldRelationsRef[0];
      if(oldRelations == null)
      {
         oldRelations = new HashMap();
         oldRelationsRef[0] = oldRelations;
      }
      oldRelations.put(cmrField, values);
      relatedManager.scheduleCascadeDelete(values);
   }

   protected void executeDeleteSQL(String sql, Object key) throws RemoveException
   {
      Connection con = null;
      PreparedStatement ps = null;
      int rowsAffected = 0;
      try
      {
         if(log.isDebugEnabled())
            log.debug("Executing SQL: " + sql);

         // get the connection
         con = entity.getDataSource().getConnection();
         ps = con.prepareStatement(sql);

         // set the parameters
         entity.setPrimaryKeyParameters(ps, 1, key);

         // execute statement
         rowsAffected = ps.executeUpdate();
      }
      catch(Exception e)
      {
         log.error("Could not remove " + key, e);
         throw new RemoveException("Could not remove " + key);
      }
      finally
      {
         JDBCUtil.safeClose(ps);
         JDBCUtil.safeClose(con);
      }

      // check results
      if(rowsAffected == 0)
      {
         log.error("Could not remove entity " + key);
         throw new RemoveException("Could not remove entity");
      }

      if(log.isDebugEnabled())
         log.debug("Remove: Rows affected = " + rowsAffected);
   }
}
