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

import javax.ejb.EJBException;
import javax.sql.DataSource;
import javax.naming.InitialContext;
import javax.naming.NamingException;

import org.jboss.deployment.DeploymentException;
import org.jboss.ejb.EntityEnterpriseContext;

import org.jboss.ejb.plugins.cmp.jdbc.JDBCContext;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCStoreManager;
import org.jboss.ejb.plugins.cmp.jdbc.SQLUtil;

import org.jboss.ejb.plugins.cmp.bridge.EntityBridge;
import org.jboss.ejb.plugins.cmp.bridge.EntityBridgeInvocationHandler;
import org.jboss.ejb.plugins.cmp.bridge.FieldBridge;

import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCAuditMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCEntityMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCCMPFieldMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCQueryMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCRelationshipRoleMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCOptimisticLockingMetaData;
import org.jboss.proxy.compiler.Proxies;
import org.jboss.proxy.compiler.InvocationHandler;


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
 * @author <a href="mailto:heiko.rupp@bancotec.de">Heiko W. Rupp</a>
 * @version $Revision: 1.26.2.8 $
 */                            
public class JDBCEntityBridge implements EntityBridge {
   private JDBCEntityMetaData metadata;
   private JDBCStoreManager manager;

   private final DataSource dataSource;

   private final String tableName;
   
   private final ArrayList tablePostCreateCmd;

   /** is the table assumed to exist */
   private boolean tableExists;
   /** did the table previously exist? */
   private boolean tableExisted=false;
   
   private List fields;
   private Map fieldsByName;

   private List cmpFields;
   private Map cmpFieldsByName;

   private List primaryKeyFields;   

   private List cmrFields;
   private Map cmrFieldsByName;

   /** used for optimistic locking */
   private JDBCCMPFieldBridge versionField;

   // Audit fields
   private JDBCCMPFieldBridge createdPrincipalField;
   private JDBCCMPFieldBridge createdTimeField;
   private JDBCCMPFieldBridge updatedPrincipalField;
   private JDBCCMPFieldBridge updatedTimeField;

   private Map selectorsByMethod;
   
   private Map loadGroups;
   private List eagerLoadFields;
   private List lazyLoadGroups;

   public JDBCEntityBridge(
         JDBCEntityMetaData metadata, 
         JDBCStoreManager manager) throws DeploymentException {

      this.metadata = metadata;                  
      this.manager = manager;

      // initial context is used to lookup the data source
      // and key generator factory
      InitialContext ic = null;
            
      // find the datasource
      try {
         ic = new InitialContext();
         dataSource = (DataSource)ic.lookup(
               metadata.getDataSourceName());
      } catch(NamingException e) {
         throw new DeploymentException("Error: can't find data source: " + 
               metadata.getDataSourceName(), e);
      }

      tableName = SQLUtil.fixTableName(
            metadata.getDefaultTableName(),
            dataSource);

	  tablePostCreateCmd = metadata.getDefaultTablePostCreateCmd();
	  
      // CMP fields
      loadCMPFields(metadata);

      // CMR fields
      loadCMRFields(metadata);

      // create locking field
      JDBCOptimisticLockingMetaData lockMetaData = metadata.getOptimisticLocking();
      if(lockMetaData != null && lockMetaData.getLockingField() != null) {
         versionField = new JDBCCMP2xFieldBridge(
            manager, lockMetaData.getLockingField()
         );
      }

      // audit fields
      JDBCAuditMetaData auditMetaData = metadata.getAudit();
      if (auditMetaData != null)
      {
          createdPrincipalField = getCMPFieldOrCreate(auditMetaData.getCreatedPrincipalField());
          createdTimeField = getCMPFieldOrCreate(auditMetaData.getCreatedTimeField());
          updatedPrincipalField = getCMPFieldOrCreate(auditMetaData.getUpdatedPrincipalField());
          updatedTimeField = getCMPFieldOrCreate(auditMetaData.getUpdatedTimeField());
      }
      int auditCount = (createdPrincipalField == null ? 0 : 1) +
                       (createdTimeField == null ? 0 : 1) +
                       (updatedPrincipalField == null ? 0 : 1) +
                       (updatedTimeField == null ? 0 : 1);

      // all fields list
      fields = new ArrayList(
         cmpFields.size() + cmrFields.size() + (versionField == null ? 0 : 1) + auditCount
      );

      fields.addAll(cmpFields);
      fields.addAll(cmrFields);
      if(versionField != null)
         fields.add(versionField);
      if (createdPrincipalField != null && 
          cmpFieldsByName.containsKey(createdPrincipalField.getFieldName()) == false)
         fields.add(createdPrincipalField);
      if (createdTimeField != null && 
          cmpFieldsByName.containsKey(createdTimeField.getFieldName()) == false)
         fields.add(createdTimeField);
      if (updatedPrincipalField != null && 
          cmpFieldsByName.containsKey(updatedPrincipalField.getFieldName()) == false)
         fields.add(updatedPrincipalField);
      if (updatedTimeField != null && 
          cmpFieldsByName.containsKey(updatedTimeField.getFieldName()) == false)
         fields.add(updatedTimeField);
      fields = Collections.unmodifiableList(fields);
      fieldsByName = new HashMap(fields.size());
      fieldsByName.putAll(cmpFieldsByName);
      fieldsByName.putAll(cmrFieldsByName);
      fieldsByName = Collections.unmodifiableMap(fieldsByName);

      // ejbSelect methods
      loadSelectors(metadata);
   }

   public void resolveRelationships() throws DeploymentException {
      for(Iterator iter = cmrFields.iterator(); iter.hasNext();) {
         JDBCCMRFieldBridge cmrField = (JDBCCMRFieldBridge)iter.next();
         cmrField.resolveRelationship();
      }

      // load groups:  cannot be created until relationships have 
      // been resolved because loadgroups must check for foreign keys
      loadLoadGroups(metadata);
      loadEagerLoadGroup(metadata);
      loadLazyLoadGroups(metadata);
   }

   private void loadCMPFields(JDBCEntityMetaData metadata)
         throws DeploymentException {

      // map between field names and field objects
      cmpFieldsByName = new HashMap(metadata.getCMPFields().size());
      // only non pk fields are stored here at first and then later
      // the pk fields are added to the front (makes sql easier to read)
      cmpFields = new ArrayList(metadata.getCMPFields().size());
      // primary key cmp fields
      primaryKeyFields = new ArrayList(metadata.getCMPFields().size());
                              
      // create each field    
      Iterator iter = metadata.getCMPFields().iterator();
      while(iter.hasNext()) {
         JDBCCMPFieldMetaData cmpFieldMetaData = 
               (JDBCCMPFieldMetaData)iter.next();
         JDBCCMPFieldBridge cmpField = 
               createCMPField(metadata, cmpFieldMetaData);
         cmpFieldsByName.put(cmpField.getFieldName(), cmpField);
         
         if(cmpField.isPrimaryKeyMember()) {
            primaryKeyFields.add(cmpField);
         } else {
            cmpFields.add(cmpField);
         }
      
      }
      
      // save the pk fields in the pk field array
      primaryKeyFields = Collections.unmodifiableList(primaryKeyFields);
      
      // add the pk fields to the front of the cmp list, per guarantee above
      cmpFields.addAll(0, primaryKeyFields);
      
      // now cmpFields list can never be modified
      cmpFields = Collections.unmodifiableList(cmpFields);
      cmpFieldsByName = Collections.unmodifiableMap(cmpFieldsByName);
   }

   private void loadCMRFields(JDBCEntityMetaData metadata)
         throws DeploymentException {

      cmrFieldsByName = new HashMap(metadata.getRelationshipRoles().size());
      cmrFields = new ArrayList(metadata.getRelationshipRoles().size());

      // create each field    
      for(Iterator iter = metadata.getRelationshipRoles().iterator();
            iter.hasNext();) {

         JDBCRelationshipRoleMetaData relationshipRole =
               (JDBCRelationshipRoleMetaData)iter.next();
         JDBCCMRFieldBridge cmrField =
               new JDBCCMRFieldBridge(this, manager, relationshipRole); 
         cmrFields.add(cmrField);
         cmrFieldsByName.put(cmrField.getFieldName(), cmrField);
      }
      
      cmrFields = Collections.unmodifiableList(cmrFields);
      cmrFieldsByName = Collections.unmodifiableMap(cmrFieldsByName);
   }

   private void loadLoadGroups(JDBCEntityMetaData metadata)
         throws DeploymentException {
      loadGroups = new HashMap();

      // add the * load group
      ArrayList loadFields = new ArrayList(fields.size());
      for(Iterator fieldIter = fields.iterator(); fieldIter.hasNext();) {
         JDBCFieldBridge field = (JDBCFieldBridge)fieldIter.next();
         if(!field.isPrimaryKeyMember()) {
            if(field instanceof JDBCCMRFieldBridge) {
               if(((JDBCCMRFieldBridge)field).hasForeignKey()) {
                  loadFields.add(field);
               }
            } else {
               loadFields.add(field);
            }
         }
      }
      loadGroups.put("*", loadFields);
      
      // put each group in the load groups map by group name
      Iterator groupNames = metadata.getLoadGroups().keySet().iterator();
      while(groupNames.hasNext()) {
         // get the group name
         String groupName = (String)groupNames.next();
         
         // create the fields list
         loadFields = new ArrayList();

         // add each JDBCCMPFieldBridge to the fields list
         List fieldNames = metadata.getLoadGroup(groupName);
         for(Iterator iter = fieldNames.iterator(); iter.hasNext();) {
            String fieldName = (String)iter.next();
            JDBCFieldBridge field = getExistingFieldByName(fieldName);
            if(field instanceof JDBCCMRFieldBridge) {
               if(((JDBCCMRFieldBridge)field).hasForeignKey()) {
                  loadFields.add(field);
               } else {
                  throw new DeploymentException("Only CMR fields that have " +
                        "a foreign-key may be a member of a load group: " +
                        "fieldName="+fieldName);
               }
            } else {
               loadFields.add(field);
            }
         }
         loadGroups.put(groupName, Collections.unmodifiableList(loadFields));
      }
      loadGroups = Collections.unmodifiableMap(loadGroups);
   }

   private void loadEagerLoadGroup(JDBCEntityMetaData metadata)
         throws DeploymentException {

      String eagerLoadGroupName = metadata.getEagerLoadGroup();
      if(eagerLoadGroupName == null) {
         eagerLoadFields = Collections.EMPTY_LIST;
      } else {
         eagerLoadFields = (List)loadGroups.get(eagerLoadGroupName);
      }
   }

   private void loadLazyLoadGroups(JDBCEntityMetaData metadata)
         throws DeploymentException {

      lazyLoadGroups = new ArrayList();
      
      Iterator lazyLoadGroupNames = metadata.getLazyLoadGroups().iterator();
      while(lazyLoadGroupNames.hasNext()) {
         String lazyLoadGroupName = (String)lazyLoadGroupNames.next();
         lazyLoadGroups.add(loadGroups.get(lazyLoadGroupName));
      }
      lazyLoadGroups = Collections.unmodifiableList(lazyLoadGroups);
   }

   private JDBCCMPFieldBridge createCMPField(
         JDBCEntityMetaData metadata,
         JDBCCMPFieldMetaData cmpFieldMetaData) throws DeploymentException {

      if(metadata.isCMP1x()) {
         return new JDBCCMP1xFieldBridge(manager, cmpFieldMetaData);
      } else {
         return new JDBCCMP2xFieldBridge(manager, cmpFieldMetaData);
      }
   }
   
   private void loadSelectors(JDBCEntityMetaData metadata)
         throws DeploymentException {
            
      // Don't know if this is the best way to do this.  Another way would be 
      // to deligate seletors to the JDBCFindEntitiesCommand, but this is
      // easier now.
      selectorsByMethod = new HashMap(metadata.getQueries().size());
      Iterator definedFinders = manager.getMetaData().getQueries().iterator();
      while(definedFinders.hasNext()) {
         JDBCQueryMetaData q = (JDBCQueryMetaData)definedFinders.next();

         if(q.getMethod().getName().startsWith("ejbSelect")) {
            selectorsByMethod.put(q.getMethod(), 
                  new JDBCSelectorBridge(manager, q));
         }
      }
      selectorsByMethod = Collections.unmodifiableMap(selectorsByMethod);
   }

   /**
    * Locates an existing field bridge with the same field name or creates a new one
    */
   private JDBCCMPFieldBridge getCMPFieldOrCreate(JDBCCMPFieldMetaData fieldMetaData)
      throws DeploymentException
   {
      JDBCCMPFieldBridge result = null;
      if (fieldMetaData != null)
      {
         result = (JDBCCMPFieldBridge) cmpFieldsByName.get(fieldMetaData.getFieldName());
         if (result == null)
            result = new JDBCCMP2xFieldBridge(manager, fieldMetaData);
      }
      return result;
   }
   
   public String getEntityName() {
      return metadata.getName();
   }

   public String getAbstractSchemaName() {
      return metadata.getAbstractSchemaName();
   }

   public JDBCEntityMetaData getMetaData() {
      return metadata;
   }

   public JDBCStoreManager getManager() {
      return manager;
   }

   /**
    * Returns the datasource for this entity.
    */
   public DataSource getDataSource() {
      return dataSource;
   }
   
   /** 
    * Does the table exists yet? This does not mean that table has been created
    * by the appilcation, or the the database metadata has been checked for the
    * existance of the table, but that at this point the table is assumed to 
    * exist.
    * @return true if the table exists
    */
   public boolean getTableExists() {
      return tableExists;
   }

   /** 
    * Sets table exists flag.
    */
   public void setTableExists(boolean tableExists) {
      this.tableExists = tableExists;
   }

   /**
    * Did the table already exist in the db?
    * We need to remember this in order to only
    * create indices on foreign-key-columns when the
    * table was initally created. 
    */
   public boolean getTableExisted() {
   	  return tableExisted;
   }
	
   public void setTableExisted(boolean existed) {
   	  tableExisted = existed;
   }
	
   public String getTableName() {
      return tableName;
   }

   public ArrayList getTablePostCreateCmd() {
   	  return tablePostCreateCmd;
   }
	
   public Class getRemoteInterface() {
      return metadata.getRemoteClass();
   }
   
   public Class getLocalInterface() {
      return metadata.getLocalClass();
   }

   public Class getPrimaryKeyClass() {
      return metadata.getPrimaryKeyClass();
   }

   public int getListCacheMax() {
      return metadata.getListCacheMax();
   }
   
   public int getFetchSize() {
      return metadata.getFetchSize();
   }

   public Object createPrimaryKeyInstance() {
      if(metadata.getPrimaryKeyFieldName() ==  null) {
         try {
            return getPrimaryKeyClass().newInstance();
         } catch(Exception e) {
            throw new EJBException("Error creating primary key instance: ", e);
         }
      }
      return null;
   }
   
   public List getPrimaryKeyFields() {
      return primaryKeyFields;
   }
   
   public List getFields() {
      return fields;
   }

   public FieldBridge getFieldByName(String name) {
      return (FieldBridge)fieldsByName.get(name);
   }

   private JDBCFieldBridge getExistingFieldByName(String name)
         throws DeploymentException {

      JDBCFieldBridge field = (JDBCFieldBridge)getFieldByName(name);
      if(field == null) {
         throw new DeploymentException("field not found: " + name);
      }
      return field;
   }
 
   public List getCMPFields() {
      return cmpFields;
   }

   public List getEagerLoadFields() {
      return eagerLoadFields;
   }

   public Iterator getLazyLoadGroups() {
      return lazyLoadGroups.iterator();
   }

   public List getLoadGroup(String name) {
      List group = (List)loadGroups.get(name);
      if(group == null) {
         throw new EJBException("Unknown load group: name=" + name);
      }
      return group;
   }

   public JDBCCMPFieldBridge getCMPFieldByName(String name) {
      return (JDBCCMPFieldBridge)cmpFieldsByName.get(name);
   }
   
   private JDBCCMPFieldBridge getExistingCMPFieldByName(String name)
         throws DeploymentException {

      JDBCCMPFieldBridge cmpField = getCMPFieldByName(name);
      if(cmpField == null) {
         throw new DeploymentException("cmpField not found: " + 
               "cmpFieldName="+name + " entityName=" + getEntityName());
      }
      return cmpField;
   }

   public List getCMRFields() {
      return cmrFields;
   }
   
   public JDBCCMRFieldBridge getCMRFieldByName(String name) {
      return (JDBCCMRFieldBridge)cmrFieldsByName.get(name);
   }

   public JDBCCMPFieldBridge getVersionField() {
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

   public Collection getSelectors() {
      return selectorsByMethod.values();
   }

   public void initInstance(EntityEnterpriseContext ctx) {
      for(Iterator iter = fields.iterator(); iter.hasNext();) {
         JDBCFieldBridge field = (JDBCFieldBridge)iter.next();
         field.initInstance(ctx);
      }
   }

   public boolean isCreated(EntityEnterpriseContext ctx) {
      return getEntityState(ctx).isCreated();
   }

   public void setCreated(EntityEnterpriseContext ctx) {
      getEntityState(ctx).isCreated = true;
   }

   public void setClean(EntityEnterpriseContext ctx) {
      for(Iterator iter = fields.iterator(); iter.hasNext();) {
         JDBCFieldBridge field = (JDBCFieldBridge)iter.next();
         field.setClean(ctx);
      }
   }

   /**
    * Returns the list of dirty fields.
    * Note: instead of CMR fields its foreign key fields are
    * included
    */
   public List getDirtyFields(EntityEnterpriseContext ctx) {
      List dirtyFields = new ArrayList(fields.size());
      for(Iterator iter = fields.iterator(); iter.hasNext();) {
         JDBCFieldBridge field = (JDBCFieldBridge)iter.next();
         if(field instanceof JDBCCMRFieldBridge) {
             List dirtyFkFields = ((JDBCCMRFieldBridge)field).getDirtyForeignKeyFields(ctx);
            if(!dirtyFkFields.isEmpty())
               dirtyFields.addAll(dirtyFkFields);
         } else {
            if(field.isDirty(ctx)) {
               dirtyFields.add(field);
            }
         }
      }
      return dirtyFields;
   }

   public void initPersistenceContext(EntityEnterpriseContext ctx) {
      // If we have an EJB 2.0 dynaymic proxy,
      // notify the handler of the assigned context.
      Object instance = ctx.getInstance();
      if(instance instanceof Proxies.ProxyTarget) {
         InvocationHandler handler = 
               ((Proxies.ProxyTarget)instance).getInvocationHandler();
         if(handler instanceof EntityBridgeInvocationHandler) {
            ((EntityBridgeInvocationHandler)handler).setContext(ctx);
         }
      }

      ctx.setPersistenceContext(new JDBCContext());
   }

   /**
    * This is only called in commit option B
    */
   public void resetPersistenceContext(EntityEnterpriseContext ctx) {
      for(Iterator iter = fields.iterator(); iter.hasNext();) {
         JDBCFieldBridge field = (JDBCFieldBridge)iter.next();
         field.resetPersistenceContext(ctx);
      }
   }
   

   public void destroyPersistenceContext(EntityEnterpriseContext ctx) {
      // If we have an EJB 2.0 dynaymic proxy,
      // notify the handler of the assigned context.
      Object instance = ctx.getInstance();
      if(instance instanceof Proxies.ProxyTarget) {
         InvocationHandler handler =
               ((Proxies.ProxyTarget)instance).getInvocationHandler();
         if(handler instanceof EntityBridgeInvocationHandler) {
            ((EntityBridgeInvocationHandler)handler).setContext(null);
         }
      }

      ctx.setPersistenceContext(null);
   }

   //
   // Commands to handle primary keys
   //
   
   public int setPrimaryKeyParameters(
         PreparedStatement ps,
         int parameterIndex,
         Object primaryKey) {      

      for(Iterator pkFields=primaryKeyFields.iterator(); pkFields.hasNext();) {
         JDBCCMPFieldBridge pkField = (JDBCCMPFieldBridge)pkFields.next();
         parameterIndex = pkField.setPrimaryKeyParameters(
               ps,
               parameterIndex,
               primaryKey);
      }
      return parameterIndex;
   }

   public int loadPrimaryKeyResults(
         ResultSet rs, 
         int parameterIndex, 
         Object[] pkRef) {

      pkRef[0] = createPrimaryKeyInstance();
      for(Iterator pkFields=primaryKeyFields.iterator(); pkFields.hasNext();) {
         JDBCCMPFieldBridge pkField = (JDBCCMPFieldBridge)pkFields.next();
         parameterIndex = pkField.loadPrimaryKeyResults(
               rs, parameterIndex, pkRef);
      }
      return parameterIndex;
   }
         
   public Object extractPrimaryKeyFromInstance(EntityEnterpriseContext ctx) {
      try {
         Object pk = null;
         for(Iterator pkFields=primaryKeyFields.iterator();
               pkFields.hasNext();) {

            JDBCCMPFieldBridge pkField = (JDBCCMPFieldBridge)pkFields.next();
            Object fieldValue = pkField.getInstanceValue(ctx);
            
            // updated pk object with return form set primary key value to
            // handle single valued non-composit pks and more complicated
            // behivors.
            pk = pkField.setPrimaryKeyValue(pk, fieldValue);
         }
         return pk;
      } catch(EJBException e) {
         // to avoid double wrap of EJBExceptions
         throw e;
      } catch(Exception e) {
         // Non recoverable internal exception
         throw new EJBException("Internal error extracting primary key from " +
               "instance", e);
      }
   }

   public void injectPrimaryKeyIntoInstance(
         EntityEnterpriseContext ctx,
         Object pk) {

      for(Iterator pkFields=primaryKeyFields.iterator(); pkFields.hasNext();) {
         JDBCCMPFieldBridge pkField = (JDBCCMPFieldBridge)pkFields.next();
         Object fieldValue = pkField.getPrimaryKeyValue(pk);
         pkField.setInstanceValue(ctx, fieldValue);
      }
   }

   public EntityState getEntityState(EntityEnterpriseContext ctx) {
      JDBCContext jdbcCtx = (JDBCContext)ctx.getPersistenceContext();
      EntityState entityState = (EntityState)jdbcCtx.get(this);
      if(entityState == null) {
         entityState = new EntityState();
         jdbcCtx.put(this, entityState);
      }
      return entityState;
   }

   public static class EntityState {
      private boolean isCreated = false;
      
      public boolean isCreated() {
         return isCreated;
      }
   }
}
