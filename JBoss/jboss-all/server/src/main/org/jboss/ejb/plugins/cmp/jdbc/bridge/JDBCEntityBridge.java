/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.cmp.jdbc.bridge;

import java.sql.PreparedStatement;
import java.sql.ResultSet;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Arrays;
import java.util.NoSuchElementException;

import javax.ejb.EJBException;
import javax.ejb.RemoveException;
import javax.sql.DataSource;
import javax.naming.InitialContext;
import javax.naming.NamingException;

import org.jboss.deployment.DeploymentException;
import org.jboss.ejb.EntityEnterpriseContext;

import org.jboss.ejb.plugins.cmp.jdbc.JDBCContext;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCStoreManager;
import org.jboss.ejb.plugins.cmp.jdbc.SQLUtil;
import org.jboss.ejb.plugins.cmp.jdbc.LockingStrategy;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCTypeFactory;

import org.jboss.ejb.plugins.cmp.bridge.EntityBridge;
import org.jboss.ejb.plugins.cmp.bridge.EntityBridgeInvocationHandler;
import org.jboss.ejb.plugins.cmp.bridge.FieldBridge;

import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCAuditMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCEntityMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCCMPFieldMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCQueryMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCRelationshipRoleMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCOptimisticLockingMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCReadAheadMetaData;
import org.jboss.proxy.compiler.Proxies;
import org.jboss.proxy.compiler.InvocationHandler;
import org.jboss.logging.Logger;


/**
 * JDBCEntityBridge follows the Bridge pattern [Gamma et. al, 1995].
 * The main job of this class is to construct the bridge from entity meta data.
 *
 * Life-cycle:
 *      Undefined. Should be tied to CMPStoreManager.
 *
 * Multiplicity:
 *      One per cmp entity bean type.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="mailto:loubyansky@ua.fm">Alex Loubyansky</a>
 * @author <a href="mailto:heiko.rupp@cellent.de">Heiko W. Rupp</a>
 * @version $Revision: 1.26.2.25 $
 */
public class JDBCEntityBridge implements EntityBridge
{
   public final static byte LOADED = 1;
   public final static byte LOAD_REQUIRED = 2;
   public final static byte DIRTY = 4;
   public final static byte CHECK_DIRTY = 8;
   public final static byte LOCKED = 16;
   public final static byte ADD_TO_SET_ON_UPDATE = 32;
   public final static byte ADD_TO_WHERE_ON_UPDATE = 64;

   private static final String DEFAULT_LOADGROUP_NAME = "*";

   private JDBCEntityMetaData metadata;
   private JDBCStoreManager manager;
   private DataSource dataSource;
   private String tableName;
   private List tablePostCreateCmd;

   /** is the table assumed to exist */
   private boolean tableExists;
   /** did the table previously exist? */
   private boolean tableExisted;

   /** primary key fields (not added to cmpFields) */
   private final String primaryKeyFieldName;
   private final Class primaryKeyClass;
   private JDBCCMPFieldBridge[] primaryKeyFields;
   /** CMP fields */
   private JDBCCMPFieldBridge[] cmpFields;
   /** CMR fields */
   private JDBCCMRFieldBridge[] cmrFields;
   /** table fields */
   private JDBCCMPFieldBridge[] tableFields;

   /** used for optimistic locking. (added to cmpFields) */
   private JDBCCMPFieldBridge versionField;

   // Audit fields (added to cmpFields)
   private JDBCCMPFieldBridge createdPrincipalField;
   private JDBCCMPFieldBridge createdTimeField;
   private JDBCCMPFieldBridge updatedPrincipalField;
   private JDBCCMPFieldBridge updatedTimeField;

   private Map selectorsByMethod;

   /** Load group is a boolean array with tableFields.length elements. True means the element is in the group. */
   private Map loadGroupMasks;
   private List lazyLoadGroupMasks;
   private boolean[] eagerLoadGroupMask;
   private boolean[] defaultLockGroupMask;

   private int jdbcContextSize;

   private final Logger log;

   public JDBCEntityBridge(JDBCEntityMetaData metadata, JDBCStoreManager manager)
      throws DeploymentException
   {
      this.metadata = metadata;
      this.manager = manager;
      primaryKeyFieldName = metadata.getPrimaryKeyFieldName();
      primaryKeyClass = metadata.getPrimaryKeyClass();
      log = Logger.getLogger(this.getClass().getName() + "." + metadata.getName());
   }

   public void init() throws DeploymentException
   {
      try
      {
         InitialContext ic = new InitialContext();
         dataSource = (DataSource)ic.lookup(metadata.getDataSourceName());
      }
      catch(NamingException e)
      {
         throw new DeploymentException("Error: can't find data source: " +
            metadata.getDataSourceName(), e);
      }

      tableName = SQLUtil.fixTableName(metadata.getDefaultTableName(), dataSource);

      tablePostCreateCmd = metadata.getDefaultTablePostCreateCmd();

      // CMP fields
      loadCMPFields(metadata);

      // CMR fields
      loadCMRFields(metadata);

      // create locking field
      JDBCOptimisticLockingMetaData lockMetaData = metadata.getOptimisticLocking();
      if(lockMetaData != null && lockMetaData.getLockingField() != null)
      {
         Integer strategy = lockMetaData.getLockingStrategy();
         JDBCCMPFieldMetaData versionMD = lockMetaData.getLockingField();

         versionField = getCMPFieldByName(versionMD.getFieldName());
         boolean hidden = versionField == null;
         if(strategy == JDBCOptimisticLockingMetaData.VERSION_COLUMN_STRATEGY)
         {
            if(hidden)
               versionField = new JDBCLongVersionFieldBridge(manager, versionMD);
            else
               versionField = new JDBCLongVersionFieldBridge((JDBCCMP2xFieldBridge)versionField);
         }
         else if(strategy == JDBCOptimisticLockingMetaData.TIMESTAMP_COLUMN_STRATEGY)
         {
            if(hidden)
               versionField = new JDBCTimestampVersionFieldBridge(manager, versionMD);
            else
               versionField = new JDBCTimestampVersionFieldBridge((JDBCCMP2xFieldBridge)versionField);
         }
         else if(strategy == JDBCOptimisticLockingMetaData.KEYGENERATOR_COLUMN_STRATEGY)
         {
            if(hidden)
               versionField = new JDBCKeyGenVersionFieldBridge(
                  manager, versionMD, lockMetaData.getKeyGeneratorFactory());
            else
               versionField = new JDBCKeyGenVersionFieldBridge(
                  (JDBCCMP2xFieldBridge)versionField, lockMetaData.getKeyGeneratorFactory());
         }

         if(hidden)
            addCMPField(versionField);
         else
            tableFields[versionField.getTableIndex()] = versionField;
      }

      // audit fields
      JDBCAuditMetaData auditMetaData = metadata.getAudit();
      if(auditMetaData != null)
      {
         JDBCCMPFieldMetaData auditField = auditMetaData.getCreatedPrincipalField();
         if(auditField != null)
         {
            createdPrincipalField = getCMPFieldByName(auditField.getFieldName());
            if(createdPrincipalField == null)
            {
               createdPrincipalField = new JDBCCMP2xFieldBridge(manager, auditField);
               addCMPField(createdPrincipalField);
            }
         }
         else
         {
            createdPrincipalField = null;
         }

         auditField = auditMetaData.getCreatedTimeField();
         if(auditField != null)
         {
            createdTimeField = getCMPFieldByName(auditField.getFieldName());
            if(createdTimeField == null)
            {
               createdTimeField = new JDBCCMP2xFieldBridge(manager, auditField, JDBCTypeFactory.EQUALS, false);
               addCMPField(createdTimeField);
            }
            else
            {
               // just to override state factory and check-dirty-after-get
               createdTimeField = new JDBCCMP2xFieldBridge(
                  (JDBCCMP2xFieldBridge)createdTimeField, JDBCTypeFactory.EQUALS, false);
               tableFields[createdTimeField.getTableIndex()] = createdTimeField;
            }
         }
         else
         {
            createdTimeField = null;
         }

         auditField = auditMetaData.getUpdatedPrincipalField();
         if(auditField != null)
         {
            updatedPrincipalField = getCMPFieldByName(auditField.getColumnName());
            if(updatedPrincipalField == null)
            {
               updatedPrincipalField = new JDBCCMP2xUpdatedPrincipalFieldBridge(manager, auditField);
               addCMPField(updatedPrincipalField);
            }
            else
            {
               updatedPrincipalField = new JDBCCMP2xUpdatedPrincipalFieldBridge(
                  (JDBCCMP2xFieldBridge)updatedPrincipalField);
               tableFields[updatedPrincipalField.getTableIndex()] = updatedPrincipalField;
            }
         }
         else
         {
            updatedPrincipalField = null;
         }

         auditField = auditMetaData.getUpdatedTimeField();
         if(auditField != null)
         {
            updatedTimeField = getCMPFieldByName(auditField.getFieldName());
            if(updatedTimeField == null)
            {
               updatedTimeField = new JDBCCMP2xUpdatedTimeFieldBridge(manager, auditField);
               addCMPField(updatedTimeField);
            }
            else
            {
               updatedTimeField = new JDBCCMP2xUpdatedTimeFieldBridge((JDBCCMP2xFieldBridge)updatedTimeField);
               tableFields[updatedTimeField.getTableIndex()] = updatedTimeField;
            }
         }
         else
         {
            updatedTimeField = null;
         }
      }

      // ejbSelect methods
      loadSelectors(metadata);
   }

   public void resolveRelationships() throws DeploymentException
   {
      for(int i = 0; i < cmrFields.length; ++i)
         cmrFields[i].resolveRelationship();

      // load groups:  cannot be created until relationships have
      // been resolved because loadgroups must check for foreign keys
      loadLoadGroups(metadata);
      loadEagerLoadGroup(metadata);
      loadLazyLoadGroups(metadata);
   }

   /**
    * The third phase of deployment. The method is called when relationships are already resolved.
    * @throws DeploymentException
    */
   public void start() throws DeploymentException
   {
      for(int i = 0; i < cmrFields.length; ++i)
      {
         cmrFields[i].start();
      }
   }

   public boolean removeFromRelations(EntityEnterpriseContext ctx, Object[] oldRelations)
   {
      boolean removed = false;
      for(int i = 0; i < cmrFields.length; ++i)
      {
         if(cmrFields[i].removeFromRelations(ctx, oldRelations))
            removed = true;
      }
      return removed;
   }

   public void cascadeDelete(EntityEnterpriseContext ctx, Map oldRelations)
      throws RemoveException
   {
      for(int i = 0; i < cmrFields.length; ++i)
      {
         JDBCCMRFieldBridge cmrField = cmrFields[i];
         Object value = oldRelations.get(cmrField);
         if(value != null)
            cmrField.cascadeDelete(ctx, (List)value);
      }
   }

   public String getEntityName()
   {
      return metadata.getName();
   }

   public String getAbstractSchemaName()
   {
      return metadata.getAbstractSchemaName();
   }

   public Class getRemoteInterface()
   {
      return metadata.getRemoteClass();
   }

   public Class getLocalInterface()
   {
      return metadata.getLocalClass();
   }

   public JDBCEntityMetaData getMetaData()
   {
      return metadata;
   }

   public JDBCStoreManager getManager()
   {
      return manager;
   }

   /**
    * Returns the datasource for this entity.
    */
   public DataSource getDataSource()
   {
      return dataSource;
   }

   /**
    * Does the table exists yet? This does not mean that table has been created
    * by the appilcation, or the the database metadata has been checked for the
    * existance of the table, but that at this point the table is assumed to
    * exist.
    * @return true if the table exists
    */
   public boolean getTableExists()
   {
      return tableExists;
   }

   /**
    * Sets table exists flag.
    */
   public void setTableExists(boolean tableExists)
   {
      this.tableExists = tableExists;
   }

   /**
    * Did the table already exist in the db?
    * We need to remember this in order to only
    * create indices on foreign-key-columns when the
    * table was initally created.
    */
   public boolean getTableExisted()
   {
      return tableExisted;
   }

   public void setTableExisted(boolean existed)
   {
      tableExisted = existed;
   }

   public String getTableName()
   {
      return tableName;
   }

   public List getTablePostCreateCmd()
   {
      return tablePostCreateCmd;
   }

   public Class getPrimaryKeyClass()
   {
      return primaryKeyClass;
   }

   public int getListCacheMax()
   {
      return metadata.getListCacheMax();
   }

   public int getFetchSize()
   {
      return metadata.getFetchSize();
   }

   public Object createPrimaryKeyInstance()
   {
      if(primaryKeyFieldName == null)
      {
         try
         {
            return primaryKeyClass.newInstance();
         }
         catch(Exception e)
         {
            throw new EJBException("Error creating primary key instance: ", e);
         }
      }
      return null;
   }

   public JDBCCMPFieldBridge[] getPrimaryKeyFields()
   {
      return primaryKeyFields;
   }

   /**
    * This method is called only at deployment time, not called at runtime.
    * @return the list of all the fields.
    */
   public List getFields()
   {
      int fieldsTotal = primaryKeyFields.length + cmpFields.length + cmrFields.length;
      JDBCFieldBridge[] fields = new JDBCFieldBridge[fieldsTotal];
      int position = 0;
      // primary key fields
      System.arraycopy(primaryKeyFields, 0, fields, position, primaryKeyFields.length);
      position += primaryKeyFields.length;
      // cmp fields
      System.arraycopy(cmpFields, 0, fields, position, cmpFields.length);
      position += cmpFields.length;
      // cmr fields
      System.arraycopy(cmrFields, 0, fields, position, cmrFields.length);
      return Arrays.asList(fields);
   }

   public FieldBridge getFieldByName(String name)
   {
      FieldBridge field = null;
      for(int i = 0; i < primaryKeyFields.length; ++i)
      {
         JDBCCMPFieldBridge primaryKeyField = primaryKeyFields[i];
         if(primaryKeyField.getFieldName().equals(name))
         {
            field = primaryKeyField;
            break;
         }
      }
      if(field == null)
      {
         field = getCMPFieldByName(name);
      }
      if(field == null)
      {
         field = getCMRFieldByName(name);
      }
      return field;
   }

   public boolean[] getEagerLoadMask()
   {
      return eagerLoadGroupMask;
   }

   public Iterator getLazyLoadGroupMasks()
   {
      return lazyLoadGroupMasks.iterator();
   }

   public boolean[] getLoadGroupMask(String name)
   {
      return (boolean[])loadGroupMasks.get(name);
   }

   public FieldIterator getLoadIterator(JDBCCMPFieldBridge requiredField,
                                        JDBCReadAheadMetaData readahead,
                                        EntityEnterpriseContext ctx)
   {
      boolean[] loadGroup;
      if(requiredField == null)
      {
         if(readahead != null && !readahead.isNone())
         {
            if(log.isTraceEnabled())
            {
               log.trace("Eager-load for entity: readahead=" + readahead);
            }
            loadGroup = getLoadGroupMask(readahead.getEagerLoadGroup());
         }
         else
         {
            if(log.isTraceEnabled())
            {
               log.trace("Default eager-load for entity: readahead=" + readahead);
            }
            loadGroup = eagerLoadGroupMask;
         }
      }
      else
      {
         loadGroup = new boolean[tableFields.length];
         int requiredInd = requiredField.getTableIndex();
         loadGroup[requiredInd] = true;
         for(Iterator groups = lazyLoadGroupMasks.iterator(); groups.hasNext();)
         {
            boolean[] lazyGroup = (boolean[])groups.next();
            if(lazyGroup[requiredInd])
            {
               for(int i = 0; i < loadGroup.length; ++i)
                  loadGroup[i] = loadGroup[i] || lazyGroup[i];
            }
         }
      }

      // filter
      EntityState entityState = getEntityState(ctx);
      int fieldsToLoad = 0;
      for(int i = 0; i < tableFields.length; ++i)
      {
         JDBCCMPFieldBridge field = tableFields[i];
         if(loadGroup[i] &&
            !(field.isLoaded(ctx) ||
            field.isReadOnly() && !field.isReadTimedOut(ctx) ||
            field.isPrimaryKeyMember()))
         {
            entityState.setLoadRequired(i);
            ++fieldsToLoad;
         }
      }
      return fieldsToLoad > 0 ? entityState.getLoadIterator(ctx) : EMPTY_FIELD_ITERATOR;
   }

   /**
    * @param name  CMP field name
    * @return  JDBCCMPFieldBridge instance or null if no field found.
    */
   public JDBCCMPFieldBridge getCMPFieldByName(String name)
   {
      for(int i = 0; i < primaryKeyFields.length; ++i)
      {
         JDBCCMPFieldBridge cmpField = primaryKeyFields[i];
         if(cmpField.getFieldName().equals(name))
            return cmpField;
      }
      for(int i = 0; i < cmpFields.length; ++i)
      {
         JDBCCMPFieldBridge cmpField = cmpFields[i];
         if(cmpField.getFieldName().equals(name))
            return cmpField;
      }
      return null;
   }

   public JDBCCMRFieldBridge[] getCMRFields()
   {
      return cmrFields;
   }

   private JDBCCMRFieldBridge getCMRFieldByName(String name)
   {
      for(int i = 0; i < cmrFields.length; ++i)
      {
         JDBCCMRFieldBridge cmrField = cmrFields[i];
         if(cmrField.getFieldName().equals(name))
            return cmrField;
      }
      return null;
   }

   public JDBCCMPFieldBridge getVersionField()
   {
      return versionField;
   }

   public JDBCCMPFieldBridge getCreatedPrincipalField()
   {
      return createdPrincipalField;
   }

   public JDBCCMPFieldBridge getCreatedTimeField()
   {
      return createdTimeField;
   }

   public JDBCCMPFieldBridge getUpdatedPrincipalField()
   {
      return updatedPrincipalField;
   }

   public JDBCCMPFieldBridge getUpdatedTimeField()
   {
      return updatedTimeField;
   }

   public Collection getSelectors()
   {
      return selectorsByMethod.values();
   }

   public void initInstance(EntityEnterpriseContext ctx)
   {
      for(int i = 0; i < tableFields.length; ++i)
         tableFields[i].initInstance(ctx);
      //for(int i = 0; i < primaryKeyFields.length; ++i)
      //   primaryKeyFields[i].initInstance(ctx);
      //for(int i = 0; i < cmpFields.length; ++i)
      //   cmpFields[i].initInstance(ctx);
      for(int i = 0; i < cmrFields.length; ++i)
      {
         JDBCCMRFieldBridge cmrField = cmrFields[i];
         cmrField.initInstance(ctx);
      }
   }

   public static boolean isCreated(EntityEnterpriseContext ctx)
   {
      return getEntityState(ctx).isCreated();
   }

   public static boolean isEjbCreateDone(EntityEnterpriseContext ctx)
   {
      return getEntityState(ctx).ejbCreateDone;
   }

   public static void setCreated(EntityEnterpriseContext ctx)
   {
      getEntityState(ctx).setCreated();
   }

   public static void setEjbCreateDone(EntityEnterpriseContext ctx)
   {
      getEntityState(ctx).ejbCreateDone = true;
   }

   /**
    * Returns the mask for dirty fields.
    */
   public boolean isDirty(EntityEnterpriseContext ctx)
   {
      return getEntityState(ctx).isDirty(ctx);
   }

   public FieldIterator getDirtyIterator(EntityEnterpriseContext ctx)
   {
      return getEntityState(ctx).getDirtyIterator(ctx);
   }

   public boolean hasLockedFields(EntityEnterpriseContext ctx)
   {
      return getEntityState(ctx).hasLockedFields();
   }

   public FieldIterator getLockedIterator(EntityEnterpriseContext ctx)
   {
      return getEntityState(ctx).getLockedIterator(ctx);
   }

   public void initPersistenceContext(EntityEnterpriseContext ctx)
   {
      // If we have an EJB 2.0 dynaymic proxy,
      // notify the handler of the assigned context.
      Object instance = ctx.getInstance();
      if(instance instanceof Proxies.ProxyTarget)
      {
         InvocationHandler handler = ((Proxies.ProxyTarget)instance).getInvocationHandler();
         if(handler instanceof EntityBridgeInvocationHandler)
            ((EntityBridgeInvocationHandler)handler).setContext(ctx);
      }
      ctx.setPersistenceContext(new JDBCContext(jdbcContextSize, new EntityState()));
   }

   /**
    * This is only called in commit option B
    */
   public void resetPersistenceContext(EntityEnterpriseContext ctx)
   {
      for(int i = 0; i < primaryKeyFields.length; ++i)
         primaryKeyFields[i].resetPersistenceContext(ctx);
      for(int i = 0; i < cmpFields.length; ++i)
         cmpFields[i].resetPersistenceContext(ctx);
      for(int i = 0; i < cmrFields.length; ++i)
         cmrFields[i].resetPersistenceContext(ctx);
   }


   public static void destroyPersistenceContext(EntityEnterpriseContext ctx)
   {
      // If we have an EJB 2.0 dynaymic proxy,
      // notify the handler of the assigned context.
      Object instance = ctx.getInstance();
      if(instance instanceof Proxies.ProxyTarget)
      {
         InvocationHandler handler = ((Proxies.ProxyTarget)instance).getInvocationHandler();
         if(handler instanceof EntityBridgeInvocationHandler)
            ((EntityBridgeInvocationHandler)handler).setContext(null);
      }
      ctx.setPersistenceContext(null);
   }

   //
   // Commands to handle primary keys
   //

   public int setPrimaryKeyParameters(PreparedStatement ps, int parameterIndex, Object primaryKey)
   {
      for(int i = 0; i < primaryKeyFields.length; ++i)
         parameterIndex = primaryKeyFields[i].setPrimaryKeyParameters(ps, parameterIndex, primaryKey);
      return parameterIndex;
   }

   public int loadPrimaryKeyResults(ResultSet rs, int parameterIndex, Object[] pkRef)
   {
      pkRef[0] = createPrimaryKeyInstance();
      for(int i = 0; i < primaryKeyFields.length; ++i)
         parameterIndex = primaryKeyFields[i].loadPrimaryKeyResults(rs, parameterIndex, pkRef);
      return parameterIndex;
   }

   public Object extractPrimaryKeyFromInstance(EntityEnterpriseContext ctx)
   {
      try
      {
         Object pk = null;
         for(int i = 0; i < primaryKeyFields.length; ++i)
         {
            JDBCCMPFieldBridge pkField = primaryKeyFields[i];
            Object fieldValue = pkField.getInstanceValue(ctx);

            // updated pk object with return form set primary key value to
            // handle single valued non-composit pks and more complicated behivors.
            pk = pkField.setPrimaryKeyValue(pk, fieldValue);
         }
         return pk;
      }
      catch(EJBException e)
      {
         // to avoid double wrap of EJBExceptions
         throw e;
      }
      catch(Exception e)
      {
         // Non recoverable internal exception
         throw new EJBException("Internal error extracting primary key from " +
            "instance", e);
      }
   }

   public void injectPrimaryKeyIntoInstance(EntityEnterpriseContext ctx, Object pk)
   {
      for(int i = 0; i < primaryKeyFields.length; ++i)
      {
         JDBCCMPFieldBridge pkField = primaryKeyFields[i];
         Object fieldValue = pkField.getPrimaryKeyValue(pk);
         pkField.setInstanceValue(ctx, fieldValue);
      }
   }

   int getNextJDBCContextIndex()
   {
      return jdbcContextSize++;
   }

   int addTableField(JDBCCMPFieldBridge field)
   {
      JDBCCMPFieldBridge[] tmpFields = tableFields;
      if(tableFields == null)
      {
         tableFields = new JDBCCMPFieldBridge[1];
      }
      else
      {
         tableFields = new JDBCCMPFieldBridge[tableFields.length + 1];
         System.arraycopy(tmpFields, 0, tableFields, 0, tmpFields.length);
      }
      int index = tableFields.length - 1;
      tableFields[index] = field;

      return index;
   }

   public JDBCCMPFieldBridge[] getTableFields()
   {
      return tableFields;
   }

   /**
    * Marks the context as removed.
    * @param ctx instance's context
    */
   public void setRemoved(EntityEnterpriseContext ctx)
   {
      getEntityState(ctx).setRemoved();
   }

   /**
    * @param ctx instance's context.
    * @return true if instance was removed.
    */
   public boolean isRemoved(EntityEnterpriseContext ctx)
   {
      return getEntityState(ctx).isRemoved();
   }

   /**
    * Marks the instance as scheduled for cascade delete (not for batch cascade delete)
    * @param ctx instance's context.
    */
   public void scheduleForCascadeDelete(EntityEnterpriseContext ctx)
   {
      getEntityState(ctx).scheduleForCascadeDelete();
      if(log.isTraceEnabled())
         log.trace("Scheduled for cascade-delete: " + ctx.getId());
   }

   /**
    * @param ctx instance's context.
    * @return true if instance was scheduled for cascade delete (not for batch cascade delete)
    */
   public boolean isScheduledForCascadeDelete(EntityEnterpriseContext ctx)
   {
      return getEntityState(ctx).isScheduledForCascadeDelete();
   }

   /**
    * Marks the instance as scheduled for batch cascade delete (not for cascade delete)
    * @param ctx instance's context.
    */
   public void scheduleForBatchCascadeDelete(EntityEnterpriseContext ctx)
   {
      getEntityState(ctx).scheduleForBatchCascadeDelete();
      if(log.isTraceEnabled())
         log.trace("Scheduled for batch-cascade-delete: " + ctx.getId());
   }

   /**
    * @param ctx instance's context.
    * @return true if instance was scheduled for batch cascade delete (not for cascade delete)
    */
   public boolean isScheduledForBatchCascadeDelete(EntityEnterpriseContext ctx)
   {
      return getEntityState(ctx).isScheduledForBatchCascadeDelete();
   }

   private static EntityState getEntityState(EntityEnterpriseContext ctx)
   {
      JDBCContext jdbcCtx = (JDBCContext)ctx.getPersistenceContext();
      EntityState entityState = jdbcCtx.getEntityState();
      if(entityState == null)
         throw new IllegalStateException("Entity state is null.");
      return entityState;
   }

   private void loadCMPFields(JDBCEntityMetaData metadata)
      throws DeploymentException
   {
      // only non pk fields are stored here at first and then later
      // the pk fields are added to the front (makes sql easier to read)
      List cmpFieldsList = new ArrayList(metadata.getCMPFields().size());
      // primary key cmp fields
      List pkFieldsList = new ArrayList(metadata.getCMPFields().size());

      // create each field
      Iterator iter = metadata.getCMPFields().iterator();
      while(iter.hasNext())
      {
         JDBCCMPFieldMetaData cmpFieldMetaData = (JDBCCMPFieldMetaData)iter.next();
         JDBCCMPFieldBridge cmpField = createCMPField(metadata, cmpFieldMetaData);
         if(cmpField.isPrimaryKeyMember())
            pkFieldsList.add(cmpField);
         else
            cmpFieldsList.add(cmpField);
      }

      // save the pk fields in the pk field array
      primaryKeyFields = new JDBCCMPFieldBridge[pkFieldsList.size()];
      for(int i = 0; i < pkFieldsList.size(); ++i)
         primaryKeyFields[i] = (JDBCCMPFieldBridge)pkFieldsList.get(i);

      // add the pk fields to the front of the cmp list, per guarantee above
      cmpFields = new JDBCCMPFieldBridge[metadata.getCMPFields().size() - primaryKeyFields.length];
      int cmpFieldIndex = 0;
      for(int i = 0; i < cmpFieldsList.size(); ++i)
         cmpFields[cmpFieldIndex++] = (JDBCCMPFieldBridge)cmpFieldsList.get(i);
   }

   private void loadCMRFields(JDBCEntityMetaData metadata)
      throws DeploymentException
   {
      cmrFields = new JDBCCMRFieldBridge[metadata.getRelationshipRoles().size()];
      // create each field
      int cmrFieldIndex = 0;
      for(Iterator iter = metadata.getRelationshipRoles().iterator(); iter.hasNext();)
      {
         JDBCRelationshipRoleMetaData relationshipRole = (JDBCRelationshipRoleMetaData)iter.next();
         JDBCCMRFieldBridge cmrField = new JDBCCMRFieldBridge(this, manager, relationshipRole);
         cmrFields[cmrFieldIndex++] = cmrField;
      }
   }

   private void loadLoadGroups(JDBCEntityMetaData metadata)
      throws DeploymentException
   {
      loadGroupMasks = new HashMap();

      // load optimistic locking mask and add it to all the load group masks
      JDBCOptimisticLockingMetaData olMD = metadata.getOptimisticLocking();
      if(olMD != null)
      {
         if(versionField != null)
         {
            defaultLockGroupMask = new boolean[tableFields.length];
            defaultLockGroupMask[versionField.getTableIndex()] = true;
            versionField.setLockingStrategy(LockingStrategy.VERSION);
         }
         else if(olMD.getGroupName() != null)
         {
            defaultLockGroupMask = loadGroupMask(olMD.getGroupName(), null);
            for(int i = 0; i < tableFields.length; ++i)
            {
               if(defaultLockGroupMask[i])
               {
                  JDBCCMPFieldBridge tableField = tableFields[i];
                  tableField.setLockingStrategy(LockingStrategy.GROUP);
                  tableField.addDefaultFlag(ADD_TO_WHERE_ON_UPDATE);
               }
            }
         }
         else // read or modified strategy
         {
            LockingStrategy strategy =
               (olMD.getLockingStrategy() == JDBCOptimisticLockingMetaData.READ_STRATEGY ?
               LockingStrategy.READ : LockingStrategy.MODIFIED
               );
            for(int i = 0; i < tableFields.length; ++i)
            {
               JDBCCMPFieldBridge field = tableFields[i];
               if(!field.isPrimaryKeyMember())
                  field.setLockingStrategy(strategy);
            }
         }
      }

      // add the * load group
      boolean[] defaultLoadGroup = new boolean[tableFields.length];
      Arrays.fill(defaultLoadGroup, true);
      loadGroupMasks.put(DEFAULT_LOADGROUP_NAME, defaultLoadGroup);

      // put each group in the load groups map by group name
      Iterator groupNames = metadata.getLoadGroups().keySet().iterator();
      while(groupNames.hasNext())
      {
         // get the group name
         String groupName = (String)groupNames.next();
         boolean[] loadGroup = loadGroupMask(groupName, defaultLockGroupMask);
         loadGroupMasks.put(groupName, loadGroup);
      }
      loadGroupMasks = Collections.unmodifiableMap(loadGroupMasks);
   }

   private boolean[] loadGroupMask(String groupName, boolean[] defaultGroup)
      throws DeploymentException
   {
      List fieldNames = metadata.getLoadGroup(groupName);
      boolean[] group = new boolean[tableFields.length];
      if(defaultGroup != null)
         System.arraycopy(defaultGroup, 0, group, 0, group.length);
      for(Iterator iter = fieldNames.iterator(); iter.hasNext();)
      {
         String fieldName = (String)iter.next();
         JDBCFieldBridge field = (JDBCFieldBridge)getFieldByName(fieldName);
         if(field == null)
            throw new DeploymentException(
               "Field " + fieldName + " not found for entity " + getEntityName());

         if(field instanceof JDBCCMRFieldBridge)
         {
            if(((JDBCCMRFieldBridge)field).hasForeignKey())
            {
               JDBCCMPFieldBridge[] fkFields = ((JDBCCMRFieldBridge)field).getForeignKeyFields();
               for(int i = 0; i < fkFields.length; ++i)
               {
                  group[fkFields[i].getTableIndex()] = true;
               }
            }
            else
            {
               throw new DeploymentException("Only CMR fields that have " +
                  "a foreign-key may be a member of a load group: " +
                  "fieldName=" + fieldName);
            }
         }
         else
         {
            group[((JDBCCMPFieldBridge)field).getTableIndex()] = true;
         }
      }
      return group;
   }

   private void loadEagerLoadGroup(JDBCEntityMetaData metadata)
   {
      String eagerLoadGroupName = metadata.getEagerLoadGroup();
      if(eagerLoadGroupName == null)
         eagerLoadGroupMask = defaultLockGroupMask;
      else
         eagerLoadGroupMask = (boolean[])loadGroupMasks.get(eagerLoadGroupName);
   }

   private void loadLazyLoadGroups(JDBCEntityMetaData metadata)
   {
      List lazyGroupNames = metadata.getLazyLoadGroups();
      lazyLoadGroupMasks = new ArrayList(lazyGroupNames.size());
      for(Iterator lazyLoadGroupNames = lazyGroupNames.iterator(); lazyLoadGroupNames.hasNext();)
      {
         String lazyLoadGroupName = (String)lazyLoadGroupNames.next();
         lazyLoadGroupMasks.add(loadGroupMasks.get(lazyLoadGroupName));
      }
      lazyLoadGroupMasks = Collections.unmodifiableList(lazyLoadGroupMasks);
   }

   private JDBCCMPFieldBridge createCMPField(JDBCEntityMetaData metadata,
                                             JDBCCMPFieldMetaData cmpFieldMetaData)
      throws DeploymentException
   {
      JDBCCMPFieldBridge cmpField;
      if(metadata.isCMP1x())
         cmpField = new JDBCCMP1xFieldBridge(manager, cmpFieldMetaData);
      else
         cmpField = new JDBCCMP2xFieldBridge(manager, cmpFieldMetaData);
      return cmpField;
   }

   private void loadSelectors(JDBCEntityMetaData metadata)
   {
      // Don't know if this is the best way to do this.  Another way would be
      // to deligate seletors to the JDBCFindEntitiesCommand, but this is
      // easier now.
      selectorsByMethod = new HashMap(metadata.getQueries().size());
      Iterator definedFinders = manager.getMetaData().getQueries().iterator();
      while(definedFinders.hasNext())
      {
         JDBCQueryMetaData q = (JDBCQueryMetaData)definedFinders.next();
         if(q.getMethod().getName().startsWith("ejbSelect"))
            selectorsByMethod.put(q.getMethod(), new JDBCSelectorBridge(manager, q));
      }
      selectorsByMethod = Collections.unmodifiableMap(selectorsByMethod);
   }

   private void addCMPField(JDBCCMPFieldBridge field)
   {
      JDBCCMPFieldBridge[] tmpCMPFields = cmpFields;
      cmpFields = new JDBCCMPFieldBridge[cmpFields.length + 1];
      System.arraycopy(tmpCMPFields, 0, cmpFields, 0, tmpCMPFields.length);
      cmpFields[tmpCMPFields.length] = field;
   }

   public class EntityState
   {
      private static final byte REMOVED = 1;
      private static final byte SCHEDULED_FOR_CASCADE_DELETE = 2;
      private static final byte SCHEDULED_FOR_BATCH_CASCADE_DELETE = 4;

      /** indicates whether ejbCreate method was executed */
      private boolean ejbCreateDone = false;
      /** indicates whether ejbPostCreate method was executed */
      private boolean ejbPostCreateDone = false;

      private byte entityFlags;

      /** array of field flags*/
      private final byte[] fieldFlags = new byte[tableFields.length];

      public EntityState()
      {
         for(int i = 0; i < tableFields.length; ++i)
         {
            fieldFlags[i] = tableFields[i].getDefaultFlags();
         }
      }

      public void setRemoved()
      {
         entityFlags |= REMOVED;
         entityFlags &= ~(SCHEDULED_FOR_CASCADE_DELETE | SCHEDULED_FOR_BATCH_CASCADE_DELETE);
      }

      public boolean isRemoved()
      {
         return (entityFlags & REMOVED) > 0;
      }

      public void scheduleForCascadeDelete()
      {
         entityFlags |= SCHEDULED_FOR_CASCADE_DELETE;
      }

      public boolean isScheduledForCascadeDelete()
      {
         return (entityFlags & SCHEDULED_FOR_CASCADE_DELETE) > 0;
      }

      public void scheduleForBatchCascadeDelete()
      {
         entityFlags |= SCHEDULED_FOR_BATCH_CASCADE_DELETE | SCHEDULED_FOR_CASCADE_DELETE;
      }

      public boolean isScheduledForBatchCascadeDelete()
      {
         return (entityFlags & SCHEDULED_FOR_BATCH_CASCADE_DELETE) > 0;
      }

      public void setCreated()
      {
         ejbCreateDone = true;
         ejbPostCreateDone = true;
      }

      public boolean isCreated()
      {
         return ejbCreateDone && ejbPostCreateDone;
      }

      /**
       * @param fieldIndex  index of the field
       * @return true if the field is loaded
       */
      public boolean isLoaded(int fieldIndex)
      {
         return (fieldFlags[fieldIndex] & LOADED) > 0;
      }

      /**
       * Marks the field as loaded.
       * @param fieldIndex  index of the field.
       */
      public void setLoaded(int fieldIndex)
      {
         fieldFlags[fieldIndex] |= LOADED;
      }

      /**
       * Marks the field to be loaded.
       * @param fieldIndex  index of the field.
       */
      public void setLoadRequired(int fieldIndex)
      {
         fieldFlags[fieldIndex] |= LOAD_REQUIRED;
      }

      /**
       * The field will be checked for dirty state at commit.
       * @param fieldIndex  index of the field.
       */
      public void setCheckDirty(int fieldIndex)
      {
         fieldFlags[fieldIndex] |= CHECK_DIRTY;
      }

      /**
       * Marks the field as clean.
       * @param fieldIndex  nextIndex of the field.
       */
      public void setClean(int fieldIndex)
      {
         fieldFlags[fieldIndex] &= ~(CHECK_DIRTY | DIRTY | LOCKED);
      }

      /**
       * Resets field flags.
       * @param fieldIndex  nextIndex of the field.
       */
      public void resetFlags(int fieldIndex)
      {
         fieldFlags[fieldIndex] = tableFields[fieldIndex].getDefaultFlags();
      }

      public boolean isDirty(EntityEnterpriseContext ctx)
      {
         int dirtyFields = 0;
         for(int i = 0; i < fieldFlags.length; ++i)
         {
            if((fieldFlags[i] & CHECK_DIRTY) > 0 && tableFields[i].isDirty(ctx))
            {
               fieldFlags[i] |= DIRTY;
               ++dirtyFields;
            }
         }
         return dirtyFields > 0;
      }

      public FieldIterator getDirtyIterator(EntityEnterpriseContext ctx)
      {
         return new MaskFieldIterator(DIRTY | ADD_TO_SET_ON_UPDATE);
      }

      public boolean hasLockedFields()
      {
         boolean result = false;
         for(int i = 0; i < fieldFlags.length; ++i)
         {
            if((fieldFlags[i] & (LOCKED | ADD_TO_WHERE_ON_UPDATE)) > 0)
            {
               result = true;
               break;
            }
         }
         return result;
      }

      public FieldIterator getLockedIterator(EntityEnterpriseContext ctx)
      {
         return new MaskFieldIterator(LOCKED | ADD_TO_WHERE_ON_UPDATE);
      }

      public boolean lockValue(int fieldIndex)
      {
         boolean lock = false;
         byte fieldFlag = fieldFlags[fieldIndex];
         if((fieldFlag & LOADED) > 0 && (fieldFlag & LOCKED) == 0)
         {
            fieldFlags[fieldIndex] |= LOCKED;
            lock = true;
         }
         return lock;
      }

      public FieldIterator getLoadIterator(EntityEnterpriseContext ctx)
      {
         return new MaskFieldIterator(LOAD_REQUIRED);
      }

      // Inner

      private class MaskFieldIterator implements FieldIterator
      {
         private final int flagMask;
         private int nextIndex = 0;
         private int curIndex = -1;

         public MaskFieldIterator(int flagMask)
         {
            this.flagMask = flagMask;
         }

         public boolean hasNext()
         {
            boolean hasNext;
            while((hasNext = nextIndex < fieldFlags.length)
               && (fieldFlags[nextIndex] & flagMask) == 0)
               ++nextIndex;
            return hasNext;
         }

         public JDBCCMPFieldBridge next()
         {
            if(!hasNext())
               throw new NoSuchElementException();
            curIndex = nextIndex;
            return tableFields[nextIndex++];
         }

         public void remove()
         {
            fieldFlags[curIndex] &= ~flagMask;
         }

         public void removeAll()
         {
            int inversedMask = ~flagMask;
            for(int i = 0; i < fieldFlags.length; ++i)
               fieldFlags[i] &= inversedMask;
         }

         public void reset()
         {
            nextIndex = 0;
            curIndex = -1;
         }
      }
   }

   public static final FieldIterator EMPTY_FIELD_ITERATOR = new FieldIterator()
   {
      public boolean hasNext()
      {
         return false;
      }

      public JDBCCMPFieldBridge next()
      {
         throw new NoSuchElementException();
      }

      public void remove()
      {
         throw new UnsupportedOperationException();
      }

      public void removeAll()
      {
         throw new UnsupportedOperationException();
      }

      public void reset()
      {
      }
   };

   public static interface FieldIterator
   {
      /**
       * @return true if there are more fields to iterate through.
       */
      boolean hasNext();

      /**
       * @return the next field.
       */
      JDBCCMPFieldBridge next();

      /**
       * Removes the current field from the iterator (not from the underlying array or another source)
       */
      void remove();

      /**
       * Removes all the fields from the iterator (not from the underlying array or another source).
       */
      void removeAll();

      /**
       * Resets the current position to the first field.
       */
      void reset();
   }
}
