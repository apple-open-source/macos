/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc.metadata;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;

import org.w3c.dom.Element;

import org.jboss.deployment.DeploymentException;
import org.jboss.metadata.ApplicationMetaData;
import org.jboss.metadata.BeanMetaData;
import org.jboss.metadata.EntityMetaData;
import org.jboss.metadata.MetaData;
import org.jboss.metadata.RelationMetaData;

import org.jboss.ejb.plugins.cmp.jdbc.SQLUtil;

/**
 * This immutable class contains information about the application
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="sebastien.alborini@m4x.org">Sebastien Alborini</a>
 * @author <a href="alex@jboss.org">Alexey Loubyansky</a>
 * @version $Revision: 1.16.4.6 $
 */
public final class JDBCApplicationMetaData
{
   private final static Class JDBC_PM = org.jboss.ejb.plugins.cmp.jdbc.JDBCStoreManager.class;

   /**
    * The class loader for this application.  The class loader is used to
    * load all classes used by this application.
    */
   private final ClassLoader classLoader;

   /**
    * Application metadata loaded from the ejb-jar.xml file
    */
   private final ApplicationMetaData applicationMetaData;

   /**
    * Map with user defined type mapping, e.g. enum mappings
    */
   private final Map userTypeMappings;

   /**
    * Map of the type mappings by name.
    */
   private final Map typeMappings = new HashMap();

   /**
    * Map of the entities managed by jbosscmp-jdbc by bean name.
    */
   private final Map entities = new HashMap();

   /**
    * Collection of relations in this application.
    */
   private final Collection relationships = new HashSet();

   /**
    * Map of the collection relationship roles for each entity by entity object.
    */
   private final Map entityRoles = new HashMap();

   /**
    * Map of the dependent value classes by java class type.
    */
   private final Map valueClasses = new HashMap();

   /**
    * Map from abstract schema name to entity name
    */
   private final Map entitiesByAbstractSchemaName = new HashMap();

   /**
    * Map from entity interface(s) java type to entity name
    */
   private final Map entitiesByInterface = new HashMap();

   /**
    * Map of the entity commands by name.
    */
   private final Map entityCommands = new HashMap();

   /**
    * Constructs jdbc application meta data with the data from the
    * applicationMetaData.
    *
    * @param applicationMetaData the application data loaded from
    *    the ejb-jar.xml file
    * @param classLoader the ClassLoader used to load the classes
    *    of the application
    * @throws DeploymentException if an problem occures while loading
    *    the classes or if data in the ejb-jar.xml is inconsistent
    *    with data from jbosscmp-jdbc.xml file
    */
   public JDBCApplicationMetaData(ApplicationMetaData applicationMetaData, ClassLoader classLoader)
      throws DeploymentException
   {
      // the classloader is the same for all the beans in the application
      this.classLoader = classLoader;
      this.applicationMetaData = applicationMetaData;

      // create metadata for all jbosscmp-jdbc-managed cmp entities
      // we do that here in case there is no jbosscmp-jdbc.xml
      Iterator beans = applicationMetaData.getEnterpriseBeans();
      while(beans.hasNext())
      {
         BeanMetaData bean = (BeanMetaData)beans.next();

         // only take entities
         if(bean.isEntity())
         {
            EntityMetaData entity = (EntityMetaData)bean;

            // only take jbosscmp-jdbc-managed CMP entities
            Class pm;
            try
            {
               pm = classLoader.loadClass(entity.getContainerConfiguration().getPersistenceManager());
            }
            catch (ClassNotFoundException e)
            {
               throw new DeploymentException("Unable to load persistence manager", e);
            }
            if(entity.isCMP() && JDBC_PM.isAssignableFrom(pm))
            {
               JDBCEntityMetaData jdbcEntity = new JDBCEntityMetaData(this, entity);

               entities.put(entity.getEjbName(), jdbcEntity);

               String schemaName = jdbcEntity.getAbstractSchemaName();
               if(schemaName != null)
               {
                  entitiesByAbstractSchemaName.put(schemaName, jdbcEntity);
               }

               Class remote = jdbcEntity.getRemoteClass();
               if(remote != null)
               {
                  entitiesByInterface.put(remote, jdbcEntity);
               }

               Class local = jdbcEntity.getLocalClass();
               if(local != null)
               {
                  entitiesByInterface.put(local, jdbcEntity);
               }

               // initialized the entity roles collection
               entityRoles.put(entity.getEjbName(), new HashSet());
            }
         }
      }

      // relationships
      Iterator iterator = applicationMetaData.getRelationships();
      while(iterator.hasNext())
      {
         RelationMetaData relation = (RelationMetaData)iterator.next();

         // Relationship metadata
         JDBCRelationMetaData jdbcRelation =
            new JDBCRelationMetaData(this, relation);
         relationships.add(jdbcRelation);

         // Left relationship-role metadata
         JDBCRelationshipRoleMetaData left =
            jdbcRelation.getLeftRelationshipRole();
         Collection leftEntityRoles =
            (Collection)entityRoles.get(left.getEntity().getName());
         leftEntityRoles.add(left);

         // Right relationship-role metadata
         JDBCRelationshipRoleMetaData right =
            jdbcRelation.getRightRelationshipRole();
         Collection rightEntityRoles =
            (Collection)entityRoles.get(right.getEntity().getName());
         rightEntityRoles.add(right);
      }

      userTypeMappings = Collections.EMPTY_MAP;
   }

   /**
    * Constructs application meta data with the data contained in the
    * jboss-cmp xml element from a jbosscmp-jdbc xml file. Optional values
    * of the xml element that are not present are loaded from the
    * defalutValues parameter.
    *
    * @param element the xml Element which contains the metadata about
    *    this application
    * @param defaultValues the JDBCApplicationMetaData which contains
    *    the values
    *      for optional elements of the element
    * @throws DeploymentException if the xml element is not semantically correct
    */
   public JDBCApplicationMetaData(Element element, JDBCApplicationMetaData defaultValues)
      throws DeploymentException
   {
      // importXml will be called at least once: with standardjbosscmp-jdbc.xml
      // it may be called a second time with user-provided jbosscmp-jdbc.xml
      // we must ensure to set all defaults values in the first call

      classLoader = defaultValues.classLoader;
      applicationMetaData = defaultValues.applicationMetaData;

      Element userTypeMaps = MetaData.getOptionalChild(element, "user-type-mappings");
      if(userTypeMaps != null)
      {
         userTypeMappings = new HashMap();
         Iterator iter = MetaData.getChildrenByTagName(userTypeMaps, "user-type-mapping");
         while(iter.hasNext())
         {
            Element userTypeMappingEl = (Element)iter.next();
            JDBCUserTypeMappingMetaData userTypeMapping = new JDBCUserTypeMappingMetaData(userTypeMappingEl);
            userTypeMappings.put(userTypeMapping.getJavaType(), userTypeMapping);
         }
      }
      else
         userTypeMappings = defaultValues.getUserTypeMappings();

      // type-mappings: (optional, always set in standardjbosscmp-jdbc.xml)
      typeMappings.putAll(defaultValues.typeMappings);
      Element typeMaps = MetaData.getOptionalChild(element, "type-mappings");
      if(typeMaps != null)
      {
         for(Iterator i = MetaData.getChildrenByTagName(typeMaps, "type-mapping"); i.hasNext();)
         {
            Element typeMappingElement = (Element)i.next();
            JDBCTypeMappingMetaData typeMapping =
               new JDBCTypeMappingMetaData(typeMappingElement);
            typeMappings.put(typeMapping.getName(), typeMapping);
         }
      }

      // dependent-value-objects
      valueClasses.putAll(defaultValues.valueClasses);
      Element valueClassesElement =
         MetaData.getOptionalChild(element, "dependent-value-classes");
      if(valueClassesElement != null)
      {
         for(Iterator i = MetaData.getChildrenByTagName(
            valueClassesElement, "dependent-value-class");
             i.hasNext();)
         {

            Element valueClassElement = (Element)i.next();
            JDBCValueClassMetaData valueClass =
               new JDBCValueClassMetaData(valueClassElement, classLoader);
            valueClasses.put(valueClass.getJavaType(), valueClass);
         }
      }

      // entity-commands: (optional, always set in standardjbosscmp-jdbc.xml)
      entityCommands.putAll(defaultValues.entityCommands);
      Element entityCommandMaps = MetaData.getOptionalChild(
         element, "entity-commands");
      if(entityCommandMaps != null)
      {
         for(Iterator i =
            MetaData.getChildrenByTagName(entityCommandMaps, "entity-command");
             i.hasNext();)
         {

            Element entityCommandElement = (Element)i.next();
            JDBCEntityCommandMetaData entityCommand =
               new JDBCEntityCommandMetaData(entityCommandElement);
            entityCommands.put(entityCommand.getCommandName(), entityCommand);
         }
      }
      
      // reserved words: (optional, always set in standardjbosscmp-jdbc.xml)
      // list of reserved words that should be escaped in table names 
      Element rWords = MetaData.getOptionalChild(element,"reserved-words");
      if (rWords!=null) 
      {
      	for (Iterator i = MetaData.getChildrenByTagName(rWords,"word"); i.hasNext() ; ) 
      	{
      			Element rWord = (Element)i.next();
      			SQLUtil.addToRwords(MetaData.getElementContent(rWord));      				
      	}
      }

      // defaults: apply defaults for entities (optional, always
      // set in standardjbosscmp-jdbc.xml)
      entities.putAll(defaultValues.entities);
      entitiesByAbstractSchemaName.putAll(
         defaultValues.entitiesByAbstractSchemaName);
      entitiesByInterface.putAll(defaultValues.entitiesByInterface);
      Element defaults = MetaData.getOptionalChild(element, "defaults");
      if(defaults != null)
      {
         ArrayList values = new ArrayList(entities.values());
         for(Iterator i = values.iterator(); i.hasNext();)
         {
            JDBCEntityMetaData entityMetaData = (JDBCEntityMetaData)i.next();

            // create the new metadata with the defaults applied
            entityMetaData =
               new JDBCEntityMetaData(this, defaults, entityMetaData);

            // replace the old meta data with the new
            entities.put(entityMetaData.getName(), entityMetaData);

            String schemaName = entityMetaData.getAbstractSchemaName();
            if(schemaName != null)
            {
               entitiesByAbstractSchemaName.put(schemaName, entityMetaData);
            }

            Class remote = entityMetaData.getRemoteClass();
            if(remote != null)
            {
               entitiesByInterface.put(remote, entityMetaData);
            }

            Class local = entityMetaData.getLocalClass();
            if(local != null)
            {
               entitiesByInterface.put(local, entityMetaData);
            }
         }
      }

      // enterprise-beans: apply entity specific configuration
      // (only in jbosscmp-jdbc.xml)
      Element enterpriseBeans =
         MetaData.getOptionalChild(element, "enterprise-beans");
      if(enterpriseBeans != null)
      {
         for(Iterator i =
            MetaData.getChildrenByTagName(enterpriseBeans, "entity");
             i.hasNext();)
         {

            Element beanElement = (Element)i.next();

            // get entity by name, if not found, it is a config error
            String ejbName =
               MetaData.getUniqueChildContent(beanElement, "ejb-name");
            JDBCEntityMetaData entityMetaData = getBeanByEjbName(ejbName);

            if(entityMetaData == null)
            {
               throw new DeploymentException("Configuration found in " +
                  "jbosscmp-jdbc.xml for entity " + ejbName + " but bean " +
                  "is not a jbosscmp-jdbc-managed cmp entity in " +
                  "ejb-jar.xml");
            }
            entityMetaData =
               new JDBCEntityMetaData(this, beanElement, entityMetaData);
            entities.put(entityMetaData.getName(), entityMetaData);

            String schemaName = entityMetaData.getAbstractSchemaName();
            if(schemaName != null)
            {
               entitiesByAbstractSchemaName.put(schemaName, entityMetaData);
            }

            Class remote = entityMetaData.getRemoteClass();
            if(remote != null)
            {
               entitiesByInterface.put(remote, entityMetaData);
            }

            Class local = entityMetaData.getLocalClass();
            if(local != null)
            {
               entitiesByInterface.put(local, entityMetaData);
            }
         }
      }

      // defaults: apply defaults for relationships (optional, always
      // set in standardjbosscmp-jdbc.xml)
      if(defaults == null)
      {
         // no defaults just copy over the existing relationships and roles
         relationships.addAll(defaultValues.relationships);
         entityRoles.putAll(defaultValues.entityRoles);
      }
      else
      {

         // create a new empty role collection for each entity
         for(Iterator i = entities.values().iterator(); i.hasNext();)
         {
            JDBCEntityMetaData entity = (JDBCEntityMetaData)i.next();
            entityRoles.put(entity.getName(), new HashSet());
         }

         // for each relationship, apply defaults and store
         for(Iterator i = defaultValues.relationships.iterator();
             i.hasNext();)
         {

            JDBCRelationMetaData relationMetaData =
               (JDBCRelationMetaData)i.next();

            // create the new metadata with the defaults applied
            relationMetaData =
               new JDBCRelationMetaData(this, defaults, relationMetaData);

            // replace the old metadata with the new
            relationships.add(relationMetaData);

            // store new left role
            JDBCRelationshipRoleMetaData left =
               relationMetaData.getLeftRelationshipRole();
            Collection leftEntityRoles =
               (Collection)entityRoles.get(left.getEntity().getName());
            leftEntityRoles.add(left);

            // store new right role
            JDBCRelationshipRoleMetaData right =
               relationMetaData.getRightRelationshipRole();
            Collection rightEntityRoles =
               (Collection)entityRoles.get(right.getEntity().getName());
            rightEntityRoles.add(right);
         }
      }

      // relationships: apply entity specific configuration
      // (only in jbosscmp-jdbc.xml)
      Element relationshipsElement =
         MetaData.getOptionalChild(element, "relationships");
      if(relationshipsElement != null)
      {

         // create a map of the relations by name (if it has a name)
         Map relationByName = new HashMap();
         for(Iterator i = relationships.iterator(); i.hasNext();)
         {
            JDBCRelationMetaData relation = (JDBCRelationMetaData)i.next();
            if(relation.getRelationName() != null)
            {
               relationByName.put(relation.getRelationName(), relation);
            }
         }


         for(Iterator i = MetaData.getChildrenByTagName(relationshipsElement, "ejb-relation"); i.hasNext();)
         {
            Element relationElement = (Element)i.next();

            // get relation by name, if not found, it is a config error
            String relationName = MetaData.getUniqueChildContent(relationElement, "ejb-relation-name");
            JDBCRelationMetaData oldRelation = (JDBCRelationMetaData)relationByName.get(relationName);

            if(oldRelation == null)
            {
               throw new DeploymentException("Configuration found in " +
                  "jbosscmp-jdbc.xml for relation " + relationName +
                  " but relation is not a jbosscmp-jdbc-managed relation " +
                  "in ejb-jar.xml");
            }
            // create new metadata with relation specific config applied
            JDBCRelationMetaData newRelation =
               new JDBCRelationMetaData(this, relationElement, oldRelation);

            // replace the old metadata with the new
            relationships.remove(oldRelation);
            relationships.add(newRelation);

            // replace the old left role with the new
            JDBCRelationshipRoleMetaData newLeft =
               newRelation.getLeftRelationshipRole();
            Collection leftEntityRoles =
               (Collection)entityRoles.get(newLeft.getEntity().getName());
            leftEntityRoles.remove(oldRelation.getLeftRelationshipRole());
            leftEntityRoles.add(newLeft);

            // replace the old right role with the new
            JDBCRelationshipRoleMetaData newRight =
               newRelation.getRightRelationshipRole();
            Collection rightEntityRoles =
               (Collection)entityRoles.get(newRight.getEntity().getName());
            rightEntityRoles.remove(oldRelation.getRightRelationshipRole());
            rightEntityRoles.add(newRight);
         }
      }

   }

   /**
    * Gets the type mapping with the specified name
    * @param name the name for the type mapping
    * @return the matching type mapping or null if not found
    */
   public JDBCTypeMappingMetaData getTypeMappingByName(String name)
   {
      return (JDBCTypeMappingMetaData)typeMappings.get(name);
   }

   /**
    * Gets the relationship roles for the entity with the specified name.
    * @param entityName the name of the entity whos roles are returned
    * @return an unmodifiable collection of JDBCRelationshipRoles
    *    of the specified entity
    */
   public Collection getRolesForEntity(String entityName)
   {
      Collection roles = (Collection)entityRoles.get(entityName);
      return Collections.unmodifiableCollection(roles);
   }

   /**
    * Gets dependent value classes that are directly managed by the container.
    * @returns an unmodifiable collection of JDBCValueClassMetaData
    */
   public Collection getValueClasses()
   {
      return Collections.unmodifiableCollection(valueClasses.values());
   }

   /**
    * Gets the classloader for this application which is used to load
    * all classes.
    * @return the ClassLoader for the application
    */
   public ClassLoader getClassLoader()
   {
      return classLoader;
   }

   /**
    * Gets the metadata for an entity bean by name.
    * @param name the name of the entity meta data to return
    * @return the entity meta data for the specified name
    */
   public JDBCEntityMetaData getBeanByEjbName(String name)
   {
      return (JDBCEntityMetaData)entities.get(name);
   }

   /**
    * Gets the entity command with the specified name
    * @param name the name for the entity-command
    * @return the matching entity command or null if not found
    */
   public JDBCEntityCommandMetaData getEntityCommandByName(String name)
   {
      return (JDBCEntityCommandMetaData)entityCommands.get(name);
   }

   public Map getUserTypeMappings()
   {
      return Collections.unmodifiableMap(userTypeMappings);
   }
}
