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
 * @version $Revision: 1.8.4.4 $
 */
public final class JDBCTypeComplex implements JDBCType {
   private final JDBCTypeComplexProperty[] properties;
   private final String[] columnNames;
   private final Class[] javaTypes;
   private final int[] jdbcTypes;
   private final String[] sqlTypes;
   private final boolean[] notNull;
   private final JDBCUtil.ResultSetReader[] resultSetReaders;
   private final Class fieldType;
   private final HashMap propertiesByName = new HashMap();

   public JDBCTypeComplex(
         JDBCTypeComplexProperty[] properties,
         Class fieldType) {

      this.properties = properties;
      this.fieldType = fieldType;

      int propNum = properties.length;
      columnNames = new String[propNum];
      javaTypes = new Class[propNum];
      jdbcTypes = new int[propNum];
      sqlTypes = new String[propNum];
      notNull = new boolean[propNum];
      resultSetReaders = new JDBCUtil.ResultSetReader[propNum];
      for(int i=0; i<properties.length; i++)
      {
         JDBCTypeComplexProperty property = properties[i];
         columnNames[i] = property.getColumnName();
         javaTypes[i] = property.getJavaType();
         jdbcTypes[i] = property.getJDBCType();
         sqlTypes[i] = property.getSQLType();
         notNull[i] = property.isNotNull();
         resultSetReaders[i] = property.getResulSetReader();
         propertiesByName.put(property.getPropertyName(), property);
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

   public Object getColumnValue(int index, Object value) {
      return getColumnValue(properties[index], value);
   }

   public Object setColumnValue(int index, Object value, Object columnValue) {
      return setColumnValue(properties[index], value, columnValue);
   }

   public JDBCUtil.ResultSetReader[] getResultSetReaders()
   {
      return resultSetReaders;
   }

   public JDBCTypeComplexProperty[] getProperties() {
      return properties;
   }

   public JDBCTypeComplexProperty getProperty(String propertyName) {
      JDBCTypeComplexProperty prop = (JDBCTypeComplexProperty)propertiesByName.get(propertyName);
      if(prop == null) {
         throw new EJBException(fieldType.getName() +
               " does not have a property named " + propertyName);
      }
      return prop;
   }

   private static Object getColumnValue(JDBCTypeComplexProperty property, Object value) {
      try {
         return property.getColumnValue(value);
      } catch(EJBException e) {
         throw e;
      } catch(Exception e) {
         throw new EJBException("Error getting column value", e);
      }
   }

   private Object setColumnValue(
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
