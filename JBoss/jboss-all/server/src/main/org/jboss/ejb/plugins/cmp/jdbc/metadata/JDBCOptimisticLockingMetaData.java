/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc.metadata;

import org.jboss.deployment.DeploymentException;
import org.jboss.metadata.MetaData;
import org.jboss.logging.Logger;
import org.w3c.dom.Element;

/**
 * Optimistick locking metadata
 *
 * @author <a href="mailto:aloubyansky@hotmail.com">Alex Loubyansky</a>
 * @version $Revision: 1.1.2.2 $
 */
public final class JDBCOptimisticLockingMetaData
{
   // Constants ---------------------------------------
   public static final Integer FIELD_GROUP_STRATEGY = new Integer(1);
   public static final Integer MODIFIED_STRATEGY = new Integer(2);
   public static final Integer READ_STRATEGY = new Integer(4);
   public static final Integer VERSION_COLUMN_STRATEGY = new Integer(8);
   public static final Integer TIMESTAMP_COLUMN_STRATEGY = new Integer(16);
   public static final Integer KEYGENERATOR_COLUMN_STRATEGY = new Integer(32);

   // Attributes --------------------------------------
   /** locking strategy */
   final private Integer lockingStrategy;

   /** group name for field group strategy */
   final private String groupName;

   /** locking field for verion- or timestamp-column strategy */
   final private JDBCCMPFieldMetaData lockingField;

   /** key generator factory */
   final private String keyGeneratorFactory;

   /** logger */
   final private Logger log;

   // Constructors ------------------------------------
   /**
    * Constructs optimistic locking metadata reading
    * optimistic-locking XML element
    */
   public JDBCOptimisticLockingMetaData(JDBCEntityMetaData entityMetaData,
                                        Element element)
      throws DeploymentException
   {
      log = Logger.getLogger(entityMetaData.getName());

      Element strategyEl;
      if((strategyEl = MetaData.getOptionalChild(element, "group-name")) != null)
      {
         lockingStrategy = FIELD_GROUP_STRATEGY;
         groupName = MetaData.getElementContent(strategyEl);
         lockingField = null;
         keyGeneratorFactory = null;

         log.debug("optimistic locking: group=" + groupName);
      }
      else if((strategyEl = MetaData.getOptionalChild(element, "modified-strategy")) != null)
      {
         lockingStrategy = MODIFIED_STRATEGY;
         groupName = null;
         lockingField = null;
         keyGeneratorFactory = null;

         log.debug("optimistic locking: modified strategy");
      }
      else if((strategyEl = MetaData.getOptionalChild(element, "read-strategy")) != null)
      {
         lockingStrategy = READ_STRATEGY;
         groupName = null;
         lockingField = null;
         keyGeneratorFactory = null;

         log.debug("optimistic locking: read strategy");
      }
      else if((strategyEl = MetaData.getOptionalChild(element, "version-column")) != null)
      {
         String fieldType = MetaData.getOptionalChildContent(element, "field-type");
         if(fieldType != null)
            throw new DeploymentException(
               "field-type is not allowed for version column. It is implicitly set to java.lang.Long."
            );

         lockingStrategy = VERSION_COLUMN_STRATEGY;
         lockingField = constructLockingField(entityMetaData, element);
         groupName = null;
         keyGeneratorFactory = null;

         log.debug("optimistic locking: version-column=" + lockingField.getFieldName());
      }
      else if((strategyEl = MetaData.getOptionalChild(element, "timestamp-column")) != null)
      {
         String fieldType = MetaData.getOptionalChildContent(element, "field-type");
         if(fieldType != null)
            throw new DeploymentException(
               "field-type is not allowed for timestamp column. It is implicitly set to java.util.Date."
            );

         lockingStrategy = TIMESTAMP_COLUMN_STRATEGY;
         lockingField = constructLockingField(entityMetaData, element);
         groupName = null;
         keyGeneratorFactory = null;

         log.debug("optimistic locking: timestamp-column=" + lockingField.getFieldName());
      }
      else if((keyGeneratorFactory =
         MetaData.getOptionalChildContent(element, "key-generator-factory")) != null)
      {
         lockingStrategy = KEYGENERATOR_COLUMN_STRATEGY;
         lockingField = constructLockingField(entityMetaData, element);
         groupName = null;

         log.debug("optimistic locking: key-generator-factory=" + keyGeneratorFactory);
      }
      else
      {
         throw new DeploymentException("Unexpected error: entity "
            + entityMetaData.getName()
            + " has unkown/incorrect optimistic locking configuration.");
      }
   }

   // Public ------------------------------------------
   public Integer getLockingStrategy()
   {
      return lockingStrategy;
   }

   public String getGroupName()
   {
      return groupName;
   }

   public JDBCCMPFieldMetaData getLockingField()
   {
      return lockingField;
   }

   public String getKeyGeneratorFactory()
   {
      return keyGeneratorFactory;
   }

   // Private -----------------------------------------
   /**
    * Constructs a locking field metadata from
    * XML element
    */
   private JDBCCMPFieldMetaData constructLockingField(
      JDBCEntityMetaData entity,
      Element element)
      throws DeploymentException
   {
      // field name
      String fieldName = MetaData.getOptionalChildContent(element, "field-name");
      if(fieldName == null || fieldName.trim().length() < 1)
         fieldName = (lockingStrategy == VERSION_COLUMN_STRATEGY ?  "version_lock" :
            (lockingStrategy == TIMESTAMP_COLUMN_STRATEGY ? "timestamp_lock" : "generated_lock"));

      // column name
      String columnName = MetaData.getOptionalChildContent(element, "column-name");
      if(columnName == null || columnName.trim().length() < 1)
         columnName = (lockingStrategy == VERSION_COLUMN_STRATEGY ?  "version_lock" :
            (lockingStrategy == TIMESTAMP_COLUMN_STRATEGY ? "timestamp_lock" : "generated_lock"));

      // field type
      Class fieldType = null;
      if(lockingStrategy == VERSION_COLUMN_STRATEGY)
         fieldType = java.lang.Long.class;
      else if(lockingStrategy == TIMESTAMP_COLUMN_STRATEGY)
         fieldType = java.util.Date.class;
      String fieldTypeStr = MetaData.getOptionalChildContent(element, "field-type");
      if(fieldTypeStr != null)
      {
         try
         {
            fieldType = Thread.currentThread().
               getContextClassLoader().loadClass(fieldTypeStr);
         }
         catch(ClassNotFoundException e)
         {
            throw new DeploymentException(
               "Could not load field type for optimistic locking field "
               + fieldName + ": " + fieldTypeStr);
         }
      }

      // JDBC/SQL Type
      int jdbcType;
      String sqlType;
      String jdbcTypeName = MetaData.getOptionalChildContent(element, "jdbc-type");
      if(jdbcTypeName != null)
      {
         jdbcType = JDBCMappingMetaData.getJdbcTypeFromName(jdbcTypeName);
         sqlType = MetaData.getUniqueChildContent(element, "sql-type");
      }
      else
      {
         jdbcType = Integer.MIN_VALUE;
         sqlType = null;
      }

      return new JDBCCMPFieldMetaData(
         entity, fieldName, fieldType, columnName, jdbcType, sqlType
      );
   }
}
