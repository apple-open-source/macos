/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.HashSet;
import java.util.List;

import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.FinderException;
import javax.ejb.RemoveException;
import javax.ejb.EJBLocalObject;
import javax.transaction.Status;
import javax.transaction.Transaction;
import javax.transaction.TransactionManager;

import org.jboss.deployment.DeploymentException;
import org.jboss.ejb.Container;
import org.jboss.ejb.EjbModule;
import org.jboss.ejb.EntityContainer;
import org.jboss.ejb.EntityPersistenceStore;
import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.ejb.plugins.cmp.ejbql.Catalog;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMPFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMRFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCEntityBridge;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCApplicationMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCEntityMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCXmlFileLoader;
import org.jboss.logging.Logger;
import org.jboss.metadata.ApplicationMetaData;
import org.jboss.tm.TransactionLocal;

/**
 * JDBCStoreManager manages storage of persistence data into a table.
 * Other then loading the initial jbosscmp-jdbc.xml file this class
 * does very little. The interesting tasks are performed by the command
 * classes.
 *
 * Life-cycle:
 *      Tied to the life-cycle of the entity container.
 *
 * Multiplicity:
 *      One per cmp entity bean. This could be less if another implementaion of
 * EntityPersistenceStore is created and thoes beans use the implementation
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 * @see org.jboss.ejb.EntityPersistenceStore
 * @version $Revision: 1.42.2.20 $
 */
public final class JDBCStoreManager implements EntityPersistenceStore
{
   /** The key used to store the tx data map. */
   private static final Object TX_DATA_KEY = "TX_DATA_KEY";
   /** The key to store the Catalog */
   private static final String CATALOG = "CATALOG";

   private static final String CREATED_MANAGERS = "CREATED_JDBCStoreManagers";
   private static final String CMP_JDBC = "CMP-JDBC";

   private EjbModule ejbModule;
   private EntityContainer container;
   private Logger log;

   private JDBCEntityMetaData metaData;
   private JDBCEntityBridge entityBridge;

   private JDBCTypeFactory typeFactory;
   private JDBCQueryManager queryManager;

   private JDBCCommandFactory commandFactory;

   private ReadAheadCache readAheadCache;

   // Manager life cycle commands
   private JDBCInitCommand initCommand;
   private JDBCStartCommand startCommand;
   private JDBCStopCommand stopCommand;
   private JDBCDestroyCommand destroyCommand;

   // Entity life cycle commands
   private JDBCCreateBeanClassInstanceCommand createBeanClassInstanceCommand;
   private JDBCInitEntityCommand initEntityCommand;
   private JDBCFindEntityCommand findEntityCommand;
   private JDBCFindEntitiesCommand findEntitiesCommand;
   private JDBCCreateCommand createEntityCommand;
   private JDBCPostCreateEntityCommand postCreateEntityCommand;
   private JDBCRemoveEntityCommand removeEntityCommand;
   private JDBCLoadEntityCommand loadEntityCommand;
   private JDBCIsModifiedCommand isModifiedCommand;
   private JDBCStoreEntityCommand storeEntityCommand;
   private JDBCActivateEntityCommand activateEntityCommand;
   private JDBCPassivateEntityCommand passivateEntityCommand;

   // commands
   private JDBCLoadRelationCommand loadRelationCommand;
   private JDBCDeleteRelationsCommand deleteRelationsCommand;
   private JDBCInsertRelationsCommand insertRelationsCommand;

   /** A Transaction manager so that we can link preloaded data to a transaction */
   private TransactionManager tm;
   private TransactionLocal txDataMap;

   /** Set of EJBLocalObject instances to be cascade-deleted excluding those that should be batch-cascade-deleted. */
   private TransactionLocal cascadeDeleteSet = new TransactionLocal()
   {
      protected Object initialValue()
      {
         return new HashSet();
      }
   };

   /**
    * Gets the container for this entity.
    * @return the container for this entity; null if container has not been set
    */
   public EntityContainer getContainer()
   {
      return container;
   }

   /**
    * Sets the container for this entity.
    * @param container the container for this entity
    * @throws ClassCastException if the container is not an instance of
    * EntityContainer
    */
   public void setContainer(Container container)
   {
      this.container = (EntityContainer)container;
      if(container != null)
      {
         ejbModule = container.getEjbModule();
         log = Logger.getLogger(
            this.getClass().getName() +
            "." +
            container.getBeanMetaData().getEjbName());
      }
      else
      {
         ejbModule = null;
      }
   }

   public JDBCEntityBridge getEntityBridge()
   {
      return entityBridge;
   }

   public JDBCTypeFactory getJDBCTypeFactory()
   {
      return typeFactory;
   }

   public JDBCEntityMetaData getMetaData()
   {
      return metaData;
   }

   public JDBCQueryManager getQueryManager()
   {
      return queryManager;
   }

   public JDBCCommandFactory getCommandFactory()
   {
      return commandFactory;
   }

   public ReadAheadCache getReadAheadCache()
   {
      return readAheadCache;
   }

   //
   // Genertic data containers
   //
   public Map getApplicationDataMap()
   {
      return ejbModule.getModuleDataMap();
   }

   public Object getApplicationData(Object key)
   {
      return ejbModule.getModuleData(key);
   }

   private void putApplicationData(Object key, Object value)
   {
      ejbModule.putModuleData(key, value);
   }

   private void removeApplicationData(Object key)
   {
      ejbModule.removeModuleData(key);
   }

   private Map getApplicationTxDataMap()
   {
      try
      {
         Transaction tx = tm.getTransaction();
         if(tx == null)
         {
            return null;
         }

         // get the txDataMap from the txMap
         Map txMap = (Map)txDataMap.get(tx);

         // do we have an existing map
         if(txMap == null)
         {
            int status = tx.getStatus();
            if(status == Status.STATUS_ACTIVE || status == Status.STATUS_PREPARING)
            {
               // create and add the new map
               txMap = new HashMap();
               txDataMap.set(tx, txMap);
            }
         }
         return txMap;
      }
      catch(EJBException e)
      {
         throw e;
      }
      catch(Exception e)
      {
         throw new EJBException("Error getting application tx data map.", e);
      }
   }

   /**
    * Schedules instances for cascade-delete
    */
   public void scheduleCascadeDelete(List instances)
   {
      Set registered = (Set)cascadeDeleteSet.get();
      registered.addAll(instances);
   }

   /**
    * Unschedules instance cascade delete.
    * @param instance  EJBLocalObject instance.
    * @return  true if the instance was scheduled for cascade deleted.
    */
   public boolean uncheduledCascadeDelete(EJBLocalObject instance)
   {
      Set registered = (Set)cascadeDeleteSet.get();
      return registered.remove(instance);
   }

   public Object getApplicationTxData(Object key)
   {
      Map map = getApplicationTxDataMap();
      if(map != null)
      {
         return map.get(key);
      }
      return null;
   }

   public void putApplicationTxData(Object key, Object value)
   {
      Map map = getApplicationTxDataMap();
      if(map != null)
      {
         map.put(key, value);
      }
   }

   private Map getEntityTxDataMap()
   {
      Map entityTxDataMap = (Map)getApplicationTxData(this);
      if(entityTxDataMap == null)
      {
         entityTxDataMap = new HashMap();
         putApplicationTxData(this, entityTxDataMap);
      }
      return entityTxDataMap;
   }

   public Object getEntityTxData(Object key)
   {
      return getEntityTxDataMap().get(key);
   }

   public void putEntityTxData(Object key, Object value)
   {
      getEntityTxDataMap().put(key, value);
   }

   public void removeEntityTxData(Object key)
   {
      getEntityTxDataMap().remove(key);
   }

   public Catalog getCatalog()
   {
      return (Catalog)getApplicationData(CATALOG);
   }

   private void initApplicationDataMap()
   {
      Map moduleData = ejbModule.getModuleDataMap();
      synchronized(moduleData)
      {
         txDataMap = (TransactionLocal)moduleData.get(TX_DATA_KEY);
         if(txDataMap == null)
         {
            txDataMap = new TransactionLocal();
            moduleData.put(TX_DATA_KEY, txDataMap);
         }
      }
   }

   /**
    * Does almost nothing because other services such
    * as JDBC data sources may not have been started.
    */
   public void create() throws Exception
   {
      // Store a reference to this manager in an application level hashtable.
      // This way in the start method other managers will be able to know
      // the other managers.
      HashMap managersMap = (HashMap)getApplicationData(CREATED_MANAGERS);
      if(managersMap == null)
      {
         managersMap = new HashMap();
         putApplicationData(CREATED_MANAGERS, managersMap);
      }
      managersMap.put(container.getBeanMetaData().getEjbName(), this);
   }

   /**
    * Bring the store to a fully initialized state
    */
   public void start() throws Exception
   {
      //
      //
      // Start Phase 1: create bridge and commands but
      // don't access other entities
      initStoreManager();


      // If all managers have been started (this is the last manager),
      // complete the other two phases of startup.
      Catalog catalog = getCatalog();
      HashMap managersMap = (HashMap)getApplicationData(CREATED_MANAGERS);
      if(catalog.getEntityCount() == managersMap.size()
         && catalog.getEJBNames().equals(managersMap.keySet()))
      {
         // Make a copy of the managers (for safty)
         ArrayList managers = new ArrayList(managersMap.values());

         // remove the managers list (it is no longer needed)
         removeApplicationData(CREATED_MANAGERS);

         //
         //
         // Start Phase 2: resolve relationships
         for(int i = 0; i < managers.size(); ++i)
         {
            JDBCStoreManager manager = (JDBCStoreManager)managers.get(i);
            manager.resolveRelationships();
         }

         //
         //
         // Start Phase 3: create tables and compile queries
         for(int i = 0; i < managers.size(); ++i)
         {
            JDBCStoreManager manager = (JDBCStoreManager)managers.get(i);
            manager.startStoreManager();
         }
      }
   }

   /**
    * Preforms as much initialization as possible without referencing
    * another entity.
    */
   private void initStoreManager() throws Exception
   {
      if(log.isDebugEnabled())
         log.debug("Initializing CMP plugin for " + container.getBeanMetaData().getEjbName());

      // get the transaction manager
      tm = container.getTransactionManager();

      // initializes the generic data containers
      initApplicationDataMap();

      // load the metadata for this entity
      metaData = loadJDBCEntityMetaData();

      // setup the type factory, which is used to map java types to sql types.
      typeFactory = new JDBCTypeFactory(
         metaData.getTypeMapping(),
         metaData.getJDBCApplication().getValueClasses(),
         metaData.getJDBCApplication().getUserTypeMappings()
      );

      // create the bridge between java land and this engine (sql land)
      entityBridge = new JDBCEntityBridge(metaData, this);
      entityBridge.init();

      // add the entity bridge to the catalog
      Catalog catalog = getCatalog();
      if(catalog == null)
      {
         catalog = new Catalog();
         putApplicationData(CATALOG, catalog);
      }
      catalog.addEntity(entityBridge);

      // create the read ahead cache
      readAheadCache = new ReadAheadCache(this);
      readAheadCache.create();

      // Set up Commands
      commandFactory = new JDBCCommandFactory(this);

      // Execute the init command
      initCommand = commandFactory.createInitCommand();
      initCommand.execute();
   }

   private void resolveRelationships() throws Exception
   {
      entityBridge.resolveRelationships();
   }

   /**
    * Brings the store manager into a completely running state.
    * This method will create the database table and compile the queries.
    */
   private void startStoreManager() throws Exception
   {
      entityBridge.start();

      // Store manager life cycle commands
      startCommand = commandFactory.createStartCommand();
      stopCommand = commandFactory.createStopCommand();
      destroyCommand = commandFactory.createDestroyCommand();

      // Entity commands
      initEntityCommand = commandFactory.createInitEntityCommand();
      createBeanClassInstanceCommand = commandFactory.createCreateBeanClassInstanceCommand();
      findEntityCommand = commandFactory.createFindEntityCommand();
      findEntitiesCommand = commandFactory.createFindEntitiesCommand();
      createEntityCommand = commandFactory.createCreateEntityCommand();
      postCreateEntityCommand = commandFactory.createPostCreateEntityCommand();
      removeEntityCommand = commandFactory.createRemoveEntityCommand();
      loadEntityCommand = commandFactory.createLoadEntityCommand();
      isModifiedCommand = commandFactory.createIsModifiedCommand();
      storeEntityCommand = commandFactory.createStoreEntityCommand();
      activateEntityCommand = commandFactory.createActivateEntityCommand();
      passivateEntityCommand = commandFactory.createPassivateEntityCommand();

      // Relation commands
      loadRelationCommand = commandFactory.createLoadRelationCommand();
      deleteRelationsCommand = commandFactory.createDeleteRelationsCommand();
      insertRelationsCommand = commandFactory.createInsertRelationsCommand();

      // Create the query manager
      queryManager = new JDBCQueryManager(this);

      // Execute the start command, creates the tables
      startCommand.execute();

      // Start the query manager. At this point is creates all of the
      // query commands. The must occure in the start phase, as
      // queries can opperate on other entities in the application, and
      // all entities are gaurenteed to be createed until the start phase.
      queryManager.start();

      readAheadCache.start();
   }

   public void stop()
   {
      // On deploy errors, sometimes CMPStoreManager was never initialized!
      if(stopCommand != null)
      {
         stopCommand.execute();
      }
      readAheadCache.stop();
   }

   public void destroy()
   {
      // On deploy errors, sometimes CMPStoreManager was never initialized!
      if(destroyCommand != null)
      {
         destroyCommand.execute();
      }

      if(readAheadCache != null)
      {
         readAheadCache.destroy();
      }

      readAheadCache = null;
      if(queryManager != null)
      {
         queryManager.clear();
      }
      queryManager = null;
      //Remove proxy from proxy map so UnifiedClassloader may be released
      if(createBeanClassInstanceCommand != null)
      {
         createBeanClassInstanceCommand.destroy();
      } // end of if ()
   }

   //
   // EJB Life Cycle Commands
   //
   /**
    * Returns a new instance of a class which implemnts the bean class.
    *
    * @return the new instance
    */
   public Object createBeanClassInstance() throws Exception
   {
      if(createBeanClassInstanceCommand == null)
         throw new IllegalStateException("createBeanClassInstanceCommand == null");
      return createBeanClassInstanceCommand.execute();
   }

   public void initEntity(EntityEnterpriseContext ctx)
   {
      initEntityCommand.execute(ctx);
   }

   public Object createEntity(Method createMethod, Object[] args, EntityEnterpriseContext ctx)
      throws CreateException
   {
      Object pk = createEntityCommand.execute(createMethod, args, ctx);
      if(pk == null)
         throw new CreateException("Primary key for created instance is null.");
      return pk;
   }

   public Object postCreateEntity(Method createMethod, Object[] args, EntityEnterpriseContext ctx)
      throws CreateException
   {
      return postCreateEntityCommand.execute(createMethod, args, ctx);
   }

   public Object findEntity(Method finderMethod,
                            Object[] args,
                            EntityEnterpriseContext ctx)
      throws FinderException
   {
      return findEntityCommand.execute(finderMethod, args, ctx);
   }

   public Collection findEntities(Method finderMethod,
                                  Object[] args,
                                  EntityEnterpriseContext ctx)
      throws FinderException
   {
      return findEntitiesCommand.execute(finderMethod, args, ctx);
   }

   public void activateEntity(EntityEnterpriseContext ctx)
   {
      activateEntityCommand.execute(ctx);
   }

   /**
    * Loads entity.
    * If entity not found NoSuchEntityException is thrown.
    * @param ctx - entity context.
    */
   public void loadEntity(EntityEnterpriseContext ctx)
   {
      loadEntity(ctx, true);
   }

   public boolean loadEntity(EntityEnterpriseContext ctx, boolean failIfNotFound)
   {
      // is any on the data already in the entity valid
      if(!ctx.isValid())
      {
         if(log.isTraceEnabled())
         {
            log.trace("RESET PERSISTENCE CONTEXT: id=" + ctx.getId());
         }
         entityBridge.resetPersistenceContext(ctx);
      }

      // mark the entity as created; if it was loading it was created
      entityBridge.setCreated(ctx);

      return loadEntityCommand.execute(ctx, failIfNotFound);
   }

   public void loadField(JDBCCMPFieldBridge field, EntityEnterpriseContext ctx)
   {
      loadEntityCommand.execute(field, ctx);
   }

   public boolean isModified(EntityEnterpriseContext ctx)
   {
      return isModifiedCommand.execute(ctx);
   }

   public void storeEntity(EntityEnterpriseContext ctx)
   {
      storeEntityCommand.execute(ctx);
      synchronizeRelationData();
   }

   private void synchronizeRelationData()
   {
      Map txData = getApplicationTxDataMap();
      if(txData == null)
      {
         return;
      }

      Iterator iterator = txData.values().iterator();
      while(iterator.hasNext())
      {
         Object obj = iterator.next();
         if(obj instanceof RelationData)
         {
            RelationData relationData = (RelationData)obj;

            // delete all removed pairs from relation table
            deleteRelations(relationData);

            // insert all added pairs into the relation table
            insertRelations(relationData);

            relationData.addedRelations.clear();
            relationData.removedRelations.clear();
            relationData.notRelatedPairs.clear();
         }
      }
   }

   public void passivateEntity(EntityEnterpriseContext ctx)
   {
      passivateEntityCommand.execute(ctx);
   }

   public void removeEntity(EntityEnterpriseContext ctx) throws RemoveException
   {
      removeEntityCommand.execute(ctx);
   }

   //
   // Relationship Commands
   //
   public Collection loadRelation(JDBCCMRFieldBridge cmrField, Object pk)
   {
      return loadRelationCommand.execute(cmrField, pk);
   }

   private void deleteRelations(RelationData relationData)
   {
      deleteRelationsCommand.execute(relationData);
   }

   private void insertRelations(RelationData relationData)
   {
      insertRelationsCommand.execute(relationData);
   }

   private JDBCEntityMetaData loadJDBCEntityMetaData()
      throws DeploymentException
   {
      ApplicationMetaData amd = container.getBeanMetaData().getApplicationMetaData();

      // Get JDBC MetaData
      JDBCApplicationMetaData jamd = (JDBCApplicationMetaData)amd.getPluginData(CMP_JDBC);

      if(jamd == null)
      {
         // we are the first cmp entity to need jbosscmp-jdbc.
         // Load jbosscmp-jdbc.xml for the whole application
         JDBCXmlFileLoader jfl = new JDBCXmlFileLoader(
            amd,
            container.getClassLoader(),
            container.getLocalClassLoader(),
            log);

         jamd = jfl.load();
         amd.addPluginData(CMP_JDBC, jamd);
      }

      // Get JDBC Bean MetaData
      String ejbName = container.getBeanMetaData().getEjbName();
      JDBCEntityMetaData metadata = jamd.getBeanByEjbName(ejbName);
      if(metadata == null)
      {
         throw new DeploymentException("No metadata found for bean " + ejbName);
      }
      return metadata;
   }
}
