/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.cmp.jdbc;

import java.util.HashMap;
import javax.ejb.EJBException;

/**
 * JDBCTypeComplex provides the mapping between a Java Bean (not an EJB)
 * and a set of columns. This class has a flattened view of the Java Bean,
 * which may contain other Java Beans.  This class simply treats the bean
 * as a set of properties, which may be in the a.b.c style. The details
 * of how this mapping is performed can be found in JDBCTypeFactory.
 *
 * This class holds a description of the columns 
 * and the properties that map to the columns. Additionally, this class
 * knows how to extract a column value from the Java Bean and how to set
 * a column value info the Java Bean. See JDBCTypeComplexProperty for 
 * details on how this is done.
 * 
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.8.4.1 $
 */
public class JDBCTypeComplex implements JDBCType {
   private JDBCTypeComplexProperty[] properties;
   private String[] columnNames;   
   private Class[] javaTypes;   
   private int[] jdbcTypes;   
   private String[] sqlTypes;
   private boolean[] notNull;
   private Class fieldType;
   private HashMap propertiesByName = new HashMap();

   public JDBCTypeComplex(
         JDBCTypeComplexProperty[] properties,
         Class fieldType) {

      this.properties = properties;
      this.fieldType = fieldType;
      
      columnNames = new String[properties.length];
      for(int i=0; i<columnNames.length; i++) {
         columnNames[i] = properties[i].getColumnName();
      }
      
      javaTypes = new Class[properties.length];
      for(int i=0; i<javaTypes.length; i++) {
         javaTypes[i] = properties[i].getJavaType();
      }
      
      jdbcTypes = new int[properties.length];
      for(int i=0; i<jdbcTypes.length; i++) {
         jdbcTypes[i] = properties[i].getJDBCType();
      }
      
      sqlTypes = new String[properties.length];
      for(int i=0; i<sqlTypes.length; i++) {
         sqlTypes[i] = properties[i].getSQLType();
      }
      
      notNull = new boolean[properties.length];
      for(int i=0; i<notNull.length; i++) {
         notNull[i] = properties[i].isNotNull();
      }

      for(int i=0; i<properties.length; i++) {
         propertiesByName.put(properties[i].getPropertyName(), properties[i]);
      }
      
   }

   public String[] getColumnNames() {
      return columnNames;
   }
   
   public Class[] getJavaTypes() {
      return javaTypes;
   }
   
   public int[] getJDBCTypes() {
      return jdbcTypes;
   }
   
   public String[] getSQLTypes() {
      return sqlTypes;
   }
   
   public boolean[] getNotNull() {
      return notNull;
   }

   public boolean[] getAutoIncrement() {
      return new boolean[] {false};
   }

   public JDBCTypeComplexProperty[] getProperties() {
      return properties;
   }

   public JDBCTypeComplexProperty getProperty(String propertyName) {
      JDBCTypeComplexProperty prop = 
            (JDBCTypeComplexProperty )propertiesByName.get(propertyName);
      if(prop == null) {
         throw new EJBException(fieldType.getName() + 
               " does not have a property named " + propertyName);
      }
      return prop;
   }
   
   public Object getColumnValue(int index, Object value) {
      return getColumnValue(properties[index], value);
   }

   public Object getColumnValue(String propertyName, Object value) {
      return getColumnValue(getProperty(propertyName), value);
   }

   private Object getColumnValue(
         JDBCTypeComplexProperty property,
         Object value) {

      try {
         return property.getColumnValue(value);
      } catch(EJBException e) {
         throw e;
      } catch(Exception e) {
         throw new EJBException("Error getting column value", e);
      }
   }

   public Object setColumnValue(int index, Object value, Object columnValue) {
      return setColumnValue(properties[index], value, columnValue);
   }

   public Object setColumnValue(
         String propertyName,
         Object value, 
         Object columnValue) {

      return setColumnValue(getProperty(propertyName), value, columnValue);
   }
   
   public Object setColumnValue(
         JDBCTypeComplexProperty property,
         Object value,
         Object columnValue) {

      if(value==null && columnValue==null) {
         // nothing to do
         return null;
      }
         
      try {
         if(value == null) {
            value = fieldType.newInstance();
         }
         return property.setColumnValue(value, columnValue);
      } catch(Exception e) {
         e.printStackTrace();
         throw new EJBException("Error setting column value", e);
      }
   }
}
