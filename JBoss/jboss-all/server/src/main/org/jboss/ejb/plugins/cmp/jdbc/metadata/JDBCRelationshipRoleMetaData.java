/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc.metadata;

import java.util.Collection;
import java.util.Collections;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

import org.jboss.deployment.DeploymentException;
import org.jboss.metadata.MetaData;
import org.jboss.metadata.RelationshipRoleMetaData;
import org.w3c.dom.Element;

/**
 * Imutable class which represents one ejb-relationship-role element found in
 * the ejb-jar.xml file's ejb-relation elements.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
 * @version $Revision: 1.16.4.11 $
 */
public final class JDBCRelationshipRoleMetaData
{
   /** Relation to which this role belongs. */
   private final JDBCRelationMetaData relationMetaData;

   /** Role name */
   private final String relationshipRoleName;

   /** Is the multiplicity one? If not, multiplicity is many. */
   private final boolean multiplicityOne;

   /** Should this role have a foreign key constraint? */
   private final boolean foreignKeyConstraint;

   /** Should this entity be deleted when related entity is deleted. */
   private final boolean cascadeDelete;

   /** Should the cascade-delete be batched. */
   private final boolean batchCascadeDelete;

   /** The entity that has this role. */
   private final JDBCEntityMetaData entity;

   /** Name of the entity's cmr field for this role. */
   private final String cmrFieldName;

   /** true if this side is navigable */
   private final boolean navigable;

   /** Type of the cmr field (i.e., collection or set) */
   private final String cmrFieldType;

   private boolean genIndex;

   /** Type of the cmr field (i.e., collection or set) */
   private final JDBCReadAheadMetaData readAhead;

   /** The other role in this relationship. */
   private JDBCRelationshipRoleMetaData relatedRole;

   /** The key fields used by this role by field name. */
   private Map keyFields;

   /** deep read ahead. */
   private boolean deepReadAhead = false;

   public JDBCRelationshipRoleMetaData(JDBCRelationMetaData relationMetaData,
                                       JDBCApplicationMetaData application,
                                       RelationshipRoleMetaData role)
      throws DeploymentException
   {
      this.relationMetaData = relationMetaData;

      relationshipRoleName = role.getRelationshipRoleName();
      multiplicityOne = role.isMultiplicityOne();
      cascadeDelete = role.isCascadeDelete();
      batchCascadeDelete = false;
      foreignKeyConstraint = false;
      readAhead = null;

      String fieldName = loadCMRFieldName(role);
      if(fieldName == null)
      {
         cmrFieldName = generateNonNavigableCMRName(role);
         navigable = false;
      }
      else
      {
         cmrFieldName = fieldName;
         navigable = true;
      }
      cmrFieldType = role.getCMRFieldType();

      // get the entity for this role
      entity = application.getBeanByEjbName(role.getEntityName());
      if(entity == null)
      {
         throw new DeploymentException("Entity: " + role.getEntityName() +
            " not found for relation: " + role.getRelationMetaData().getRelationName());
      }
   }

   public JDBCRelationshipRoleMetaData(JDBCRelationMetaData relationMetaData,
                                       JDBCApplicationMetaData application,
                                       Element element,
                                       JDBCRelationshipRoleMetaData defaultValues)
      throws DeploymentException
   {

      this.relationMetaData = relationMetaData;
      this.entity = application.getBeanByEjbName(defaultValues.getEntity().getName());

      relationshipRoleName = defaultValues.getRelationshipRoleName();
      multiplicityOne = defaultValues.isMultiplicityOne();
      cascadeDelete = defaultValues.isCascadeDelete();

      cmrFieldName = defaultValues.getCMRFieldName();
      navigable = defaultValues.isNavigable();
      cmrFieldType = defaultValues.getCMRFieldType();

      // foreign key constraint?  If not provided, keep default.
      String fkString = MetaData.getOptionalChildContent(element, "fk-constraint");
      if(fkString != null)
      {
         foreignKeyConstraint = Boolean.valueOf(fkString).booleanValue();
      }
      else
      {
         foreignKeyConstraint = defaultValues.hasForeignKeyConstraint();
      }

      // read-ahead
      Element readAheadElement = MetaData.getOptionalChild(element, "read-ahead");
      if(readAheadElement != null)
      {
         readAhead = new JDBCReadAheadMetaData(readAheadElement, entity.getReadAhead());
         deepReadAhead = readAhead.isDeepReadAhead();
      }
      else
      {
         readAhead = entity.getReadAhead();
      }

      batchCascadeDelete = MetaData.getOptionalChild(element, "batch-cascade-delete") != null;
      if(batchCascadeDelete)
      {
         if(!cascadeDelete)
         throw new DeploymentException(
            relationMetaData.getRelationName() + '/' + relationshipRoleName
            + " has batch-cascade-delete in jbosscmp-jdbc.xml but has no cascade-delete in ejb-jar.xml"
         );

         if(relationMetaData.isTableMappingStyle())
         {
            throw new DeploymentException(
               "Relationship " + relationMetaData.getRelationName()
               + " with relation-table-mapping style was setup for batch cascade-delete."
               + " Batch cascade-delete supported only for foreign key mapping style."
            );
         }
      }
   }

   public void init(JDBCRelationshipRoleMetaData relatedRole)
      throws DeploymentException
   {
      init(relatedRole, null);
   }

   public void init(JDBCRelationshipRoleMetaData relatedRole, Element element)
      throws DeploymentException
   {
      this.relatedRole = relatedRole;
      if(element == null || "defaults".equals(element.getTagName()))
      {
         keyFields = loadKeyFields();
      }
      else
      {
         keyFields = loadKeyFields(element);
      }
   }

   private static String loadCMRFieldName(RelationshipRoleMetaData role)
   {
      return role.getCMRFieldName();
   }

   private static String generateNonNavigableCMRName(RelationshipRoleMetaData role)
   {
      RelationshipRoleMetaData relatedRole = role.getRelatedRoleMetaData();
      return relatedRole.getEntityName() + "_" + relatedRole.getCMRFieldName();
   }

   /**
    * Gets the relation to which this role belongs.
    */
   public JDBCRelationMetaData getRelationMetaData()
   {
      return relationMetaData;
   }

   /**
    * Gets the name of this role.
    */
   public String getRelationshipRoleName()
   {
      return relationshipRoleName;
   }

   /**
    * Should this role use a foreign key constraint.
    * @return true if the store mananager will execute an ALTER TABLE ADD
    * CONSTRAINT statement to add a foreign key constraint.
    */
   public boolean hasForeignKeyConstraint()
   {
      return foreignKeyConstraint;
   }

   /**
    * Checks if the multiplicity is one.
    */
   public boolean isMultiplicityOne()
   {
      return multiplicityOne;
   }

   /**
    * Checks if the multiplicity is many.
    */
   public boolean isMultiplicityMany()
   {
      return !multiplicityOne;
   }

   /**
    * Should this entity be deleted when related entity is deleted.
    */
   public boolean isCascadeDelete()
   {
      return cascadeDelete;
   }

   public boolean isBatchCascadeDelete()
   {
      return batchCascadeDelete;
   }

   public boolean isDeepReadAhead()
   {
      return deepReadAhead;
   }

   /**
    * Gets the name of the entity that has this role.
    */
   public JDBCEntityMetaData getEntity()
   {
      return entity;
   }

   /**
    * Gets the name of the entity's cmr field for this role.
    */
   public String getCMRFieldName()
   {
      return cmrFieldName;
   }

   private boolean isNavigable()
   {
      return navigable;
   }

   /**
    * Gets the type of the cmr field (i.e., collection or set)
    */
   private String getCMRFieldType()
   {
      return cmrFieldType;
   }

   /**
    * Gets the related role's jdbc meta data.
    */
   public JDBCRelationshipRoleMetaData getRelatedRole()
   {
      return relationMetaData.getOtherRelationshipRole(this);
   }

   /**
    * Gets the read ahead meta data
    */
   public JDBCReadAheadMetaData getReadAhead()
   {
      return readAhead;
   }

   /**
    * Gets the key fields of this role.
    * @return an unmodifiable collection of JDBCCMPFieldMetaData objects
    */
   public Collection getKeyFields()
   {
      return Collections.unmodifiableCollection(keyFields.values());
   }

   public boolean isIndexed()
   {
      return genIndex;
   }

   /**
    * Loads the key fields for this role based on the primary keys of the
    * this entity.
    */
   private Map loadKeyFields()
   {
      // with foreign key mapping, foreign key fields are no added if
      // - it is the many side of one-to-many relationship
      // - it is the one side of one-to-one relationship and related side is not navigable
      if(relationMetaData.isForeignKeyMappingStyle())
      {
         if(isMultiplicityMany())
            return Collections.EMPTY_MAP;
         else
            if(getRelatedRole().isMultiplicityOne() && !getRelatedRole().isNavigable())
               return Collections.EMPTY_MAP;
      }

      // get all of the pk fields
      ArrayList pkFields = new ArrayList();
      for(Iterator i = entity.getCMPFields().iterator(); i.hasNext();)
      {
         JDBCCMPFieldMetaData cmpField = (JDBCCMPFieldMetaData) i.next();
         if(cmpField.isPrimaryKeyMember())
         {
            pkFields.add(cmpField);
         }
      }

      // generate a new key field for each pk field
      Map fields = new HashMap(pkFields.size());
      for(Iterator i = pkFields.iterator(); i.hasNext();)
      {
         JDBCCMPFieldMetaData cmpField = (JDBCCMPFieldMetaData) i.next();

         String columnName;
         if(relationMetaData.isTableMappingStyle())
         {
            if(entity.equals(relatedRole.getEntity()))
               columnName = getCMRFieldName();
            else
               columnName = entity.getName();
         }
         else
         {
            columnName = relatedRole.getCMRFieldName();
         }

         if(pkFields.size() > 1)
         {
            columnName += "_" + cmpField.getFieldName();
         }

         cmpField = new JDBCCMPFieldMetaData(
            entity,
            cmpField,
            columnName,
            false,
            relationMetaData.isTableMappingStyle(),
            relationMetaData.isReadOnly(),
            relationMetaData.getReadTimeOut(),
            relationMetaData.isTableMappingStyle());
         fields.put(cmpField.getFieldName(), cmpField);
      }
      return Collections.unmodifiableMap(fields);
   }

   /**
    * Loads the key fields for this role based on the primary keys of the
    * this entity and the override data from the xml element.
    */
   private Map loadKeyFields(Element element)
      throws DeploymentException
   {
      Element keysElement = MetaData.getOptionalChild(element, "key-fields");

      // no field overrides, we're done
      if(keysElement == null)
      {
         return loadKeyFields();
      }

      // load overrides
      Iterator iter = MetaData.getChildrenByTagName(keysElement, "key-field");

      // if key-fields element empty, no key should be used
      if(!iter.hasNext())
      {
         return Collections.EMPTY_MAP;
      }
      else
         if(relationMetaData.isForeignKeyMappingStyle() && isMultiplicityMany())
         {
            throw new DeploymentException("Role: " + relationshipRoleName + " with multiplicity many using " +
               "foreign-key mapping is not allowed to have key-fields");
         }

      // load the default field values
      Map defaultFields = getPrimaryKeyFields();

      // load overrides
      Map fields = new HashMap(defaultFields.size());
      while(iter.hasNext())
      {
         Element keyElement = (Element) iter.next();
         String fieldName = MetaData.getUniqueChildContent(keyElement, "field-name");

         JDBCCMPFieldMetaData cmpField = (JDBCCMPFieldMetaData) defaultFields.remove(fieldName);
         if(cmpField == null)
         {
            throw new DeploymentException(
               "Role '" + relationshipRoleName + "' on Entity Bean '" +
               entity.getName() + "' : CMP field for key not found: field " +
               "name='" + fieldName + "'");
         }
         String isIndexedtmp = MetaData.getOptionalChildContent(keyElement, "dbindex");
         boolean isIndexed;

         if(isIndexedtmp != null)
            isIndexed = true;
         else
            isIndexed = false;
         genIndex = isIndexed;


         cmpField = new JDBCCMPFieldMetaData(
            entity,
            keyElement,
            cmpField,
            false,
            relationMetaData.isTableMappingStyle(),
            relationMetaData.isReadOnly(),
            relationMetaData.getReadTimeOut(),
            relationMetaData.isTableMappingStyle());
         fields.put(cmpField.getFieldName(), cmpField);
      }

      // all fields must be overriden
      if(!defaultFields.isEmpty())
      {
         throw new DeploymentException("Mappings were not provided for all " +
            "fields: unmaped fields=" + defaultFields.keySet() +
            " in role=" + relationshipRoleName);
      }
      return Collections.unmodifiableMap(fields);
   }

   /**
    * Returns the primary key fields of the entity mapped by field name.
    */
   private Map getPrimaryKeyFields()
   {
      Map pkFields = new HashMap();
      for(Iterator cmpFieldsIter = entity.getCMPFields().iterator(); cmpFieldsIter.hasNext();)
      {
         JDBCCMPFieldMetaData cmpField = (JDBCCMPFieldMetaData) cmpFieldsIter.next();
         if(cmpField.isPrimaryKeyMember())
            pkFields.put(cmpField.getFieldName(), cmpField);
      }
      return pkFields;
   }
}
