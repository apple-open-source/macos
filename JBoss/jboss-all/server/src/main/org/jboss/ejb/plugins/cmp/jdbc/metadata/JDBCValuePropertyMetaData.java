/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc.metadata;

import java.lang.reflect.Method;
import org.jboss.deployment.DeploymentException;
import org.jboss.metadata.MetaData;
import org.w3c.dom.Element;

/**
 * Imutable class which contains information about a single dependent
 * value object property.
 *     
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.7.2.1 $
 */
public final class JDBCValuePropertyMetaData {
   private final String propertyName;
   private final Class propertyType;
   private final String columnName;
   private final String sqlType;
   private final int jdbcType;
   private final boolean notNull;
   private final Method getter;
   private final Method setter;
   
   /**
    * Constructs a value property metadata class with the data contained in 
    * the property xml element from a jbosscmp-jdbc xml file.
    *
    * @param element the xml Element which contains the metadata about
    *       this property
    * @param classType the java Class type of the value class on which this 
    *       property is defined
    * @throws DeploymentException if the xml element is not semantically correct
    */
   public JDBCValuePropertyMetaData(Element element, Class classType)
         throws DeploymentException {

      // Property name
      propertyName = MetaData.getUniqueChildContent(element, "property-name");

      // Column name
      String columnNameString = 
            MetaData.getOptionalChildContent(element, "column-name");
      if(columnNameString != null) {
         columnName = columnNameString;
      } else {
         columnName = propertyName;
      }

      // Not null
      Element notNullElement = MetaData.getOptionalChild(element, "not-null");
      notNull = (notNullElement != null);

      // Getter
      try {
         getter = classType.getMethod(toGetterName(propertyName), new Class[0]);
      } catch(Exception e) {
         throw new DeploymentException("Unable to find getter for property " +
               propertyName + " on dependent value class " + 
               classType.getName());
      }

      // get property type from getter return type
      propertyType = getter.getReturnType();
      
      // resolve setter
      try {
         setter = classType.getMethod(
               toSetterName(propertyName), 
               new Class[] { propertyType }  );
      } catch(Exception e) {
         throw new DeploymentException("Unable to find setter for property " +
               propertyName + " on dependent value class " + 
               classType.getName());
      }

      // jdbc type - optional
      String jdbcString = 
            MetaData.getOptionalChildContent(element, "jdbc-type");
      if(jdbcString != null) {
         jdbcType = JDBCMappingMetaData.getJdbcTypeFromName(jdbcString); 
         
         // sql type - required if jdbc-type specified
         sqlType = MetaData.getUniqueChildContent(element, "sql-type");
      } else {
         jdbcType = Integer.MIN_VALUE;
         sqlType = null;
      }
   }
   
   /**
    * Gets the name of this property. The name will always begin with a lower
    * case letter and is a Java Beans property name.  This is the base name of
    * the getter and setter property.
    *
    * @return the name of this property
    */
   public String getPropertyName() {
      return propertyName;
   }

   /**
    * Gets the java class type of this property. The class the the return type
    * of the getter and type of the sole argument of the setter.
    *
    * @return the java Class type of this property
    */
   public Class getPropertyType() {
      return propertyType;
   }

   /**
    * Gets the column name which this property will be persisted.
    *
    * @return the name of the column which this property will be persisted
    */
   public String getColumnName() {
      return columnName;
   }
   
   /**
    * Gets the jdbc type of this property. The jdbc type is used to retrieve
    * data from a result set and to set parameters in a prepared statement.
    *
    * @return the jdbc type of this property
    */
   public int getJDBCType() {
      return jdbcType;
   }

   /**
    * Gets the sql type of this mapping. The sql type is the sql column data 
    * type, and is used in CREATE TABLE statements. 
    *
    * @return the sql type String of this mapping
    */
   public String getSqlType() {
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
    * Gets the getter method of this property. The getter method is used to 
    * retrieve the value of this property from the value class.
    *
    * @return the Method which gets the value of this property
    */
   public Method getGetter() {
      return getter;
   }

   /**
    * Gets the setter method of this property. The setter method is used to 
    * set the value of this property in the value class.
    *
    * @return the Method which sets the value of this property
    */
   public Method getSetter() {
      return setter;
   }
   
   private static String toGetterName(String propertyName) {
      return "get" + upCaseFirstCharacter(propertyName); 
   }
   
   private static String toSetterName(String propertyName) {
      return "set" + upCaseFirstCharacter(propertyName); 
   }
   
   private static String upCaseFirstCharacter(String propertyName) {
      return Character.toUpperCase(propertyName.charAt(0)) + 
            propertyName.substring(1);
   }
}
