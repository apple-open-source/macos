/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc.metadata;

import org.jboss.deployment.DeploymentException;
import org.jboss.metadata.MetaData;

import org.w3c.dom.Element;

/**
 *   This immutable class contains information about the an overriden field property.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 *   @version $Revision: 1.6 $
 */
public final class JDBCCMPFieldPropertyMetaData {
   /**
    * the cmp field on which this property is defined
    */
   private final JDBCCMPFieldMetaData cmpField;
   
   /** 
    * name of this property
    */
   private final String propertyName;

   /**
    * the column name in the table
    */
   private final String columnName;

   /**
    * the jdbc type (see java.sql.Types), used in PreparedStatement.setParameter
    */
   private final int jdbcType;

   /**
    * the sql type, used for table creation.
    */
   private final String sqlType;
   
   /**
    * Should null values not be allowed for this property.
    */
   private final boolean notNull;

   /**
    * Constructs cmp field property meta data with the data contained in the 
    * property xml element from a jbosscmp-jdbc xml file.
    *
    * @param cmpField the JDBCCMPFieldMetaData on which this property is defined
    * @param element the xml Element which contains the metadata about this 
    *    field
    * @throws DeploymentException if the xml element is not semantically correct
    */
   public JDBCCMPFieldPropertyMetaData(
         JDBCCMPFieldMetaData cmpField,
         Element element) throws DeploymentException {

      this.cmpField = cmpField;
      
      // Property name
      propertyName = MetaData.getUniqueChildContent(element, "property-name");

      // Column name
      String columnStr = 
            MetaData.getOptionalChildContent(element, "column-name");
      if(columnStr != null) {
         columnName = columnStr;
      } else {
         columnName = null;
      }

      // jdbc type
      String jdbcStr = MetaData.getOptionalChildContent(element, "jdbc-type");
      if(jdbcStr != null) {
         jdbcType = JDBCMappingMetaData.getJdbcTypeFromName(jdbcStr);
         sqlType = MetaData.getUniqueChildContent(element, "sql-type");
      } else {
         jdbcType = Integer.MIN_VALUE;
         sqlType = null;
      }   

      // notNull
      notNull = (MetaData.getOptionalChild(element, "not-null") != null);
   }

   /**
    * Constructs cmp field property meta data based on the data contained in 
    * the defaultValues parameter but defined on the specified cmpField. This 
    * is effectly a copy constructory, except it can change the cmpField object
    * on which the property is defined.
    *
    * @param cmpField the JDBCCMPFieldMetaData on which this property is defined
    * @param defaultValues the defaultValues of this property
    * @throws DeploymentException if the xml element is not semantically correct
    */
   public JDBCCMPFieldPropertyMetaData(
         JDBCCMPFieldMetaData cmpField,
         JDBCCMPFieldPropertyMetaData defaultValues) {

      this.cmpField = cmpField;
      
      // Property name
      propertyName = defaultValues.propertyName;

      // Column name
      columnName = defaultValues.columnName;

      // jdbc type
      jdbcType = defaultValues.jdbcType;
      
      // sql type
      sqlType = defaultValues.sqlType;
      
      // not-null
      notNull = defaultValues.notNull;
   }

   /**
    * Gets the cmpField on which this property is defined
    * @return the cmpField on which this property is defined
    */
   public JDBCCMPFieldMetaData getCMPField() {
      return cmpField;
   }
    
   /**
    * Gets the name of the property to be overriden.
    */
   public String getPropertyName() {
      return propertyName;
   }

   /**
    * Gets the column name the property should use or null if the
    * column name is not overriden. 
    */
   public String getColumnName() {
      return columnName;
   }

   /**
    * Gets the JDBC type the property should use or Integer.MIN_VALUE 
    * if not overriden.
    */
   public int getJDBCType() {
      return jdbcType;
   }

   /**
    * Gets the SQL type the property should use or null 
    * if not overriden.
    */
   public String getSQLType() {
      return sqlType;
   }
   
   /**
    * Should this field allow null values?
    * @return true if this field will not allow a null value.
    */
   public boolean isNotNull() {
      return notNull;
   }

   /**
    * Compares this JDBCCMPFieldPropertyMetaData against the specified object.
    * Returns true if the objects are the same. Two 
    * JDBCCMPFieldPropertyMetaData are the same if they both have the same name
    * and are defined on the same cmpField.
    * @param o the reference object with which to compare
    * @return true if this object is the same as the object argument; false
    *    otherwise
    */
   public boolean equals(Object o) {
      if(o instanceof JDBCCMPFieldPropertyMetaData) {
         JDBCCMPFieldPropertyMetaData cmpFieldProperty = 
               (JDBCCMPFieldPropertyMetaData)o;
         return propertyName.equals(cmpFieldProperty.propertyName) && 
               cmpField.equals(cmpFieldProperty.cmpField);
      }
      return false;
   }
   
   /**
    * Returns a hashcode for this JDBCCMPFieldPropertyMetaData. The hashcode is
    * computed based on the hashCode of the declaring entity and the hashCode
    * of the fieldName
    * @return a hash code value for this object
    */
   public int hashCode() {
      int result = 17;
      result = 37*result + cmpField.hashCode();
      result = 37*result + propertyName.hashCode();
      return result;
   }

   /**
    * Returns a string describing this JDBCCMPFieldPropertyMetaData. The exact
    * details of the representation are unspecified and subject to change, but
    * the following may be regarded as typical:
    * 
    * "[JDBCCMPFieldPropertyMetaData: propertyName=line1,
    *       [JDBCCMPFieldMetaData: fieldName=address, 
    *             [JDBCEntityMetaData: entityName=UserEJB]]"
    *
    * @return a string representation of the object
    */
   public String toString() {
      return "[JDBCCMPFieldPropertyMetaData : propertyName=" +
            propertyName + ", " + cmpField + "]";
   }   
}
