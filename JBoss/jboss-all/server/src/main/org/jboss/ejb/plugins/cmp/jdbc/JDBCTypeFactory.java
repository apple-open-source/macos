/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.cmp.jdbc;

import java.lang.reflect.Method;
import java.lang.reflect.Array;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.List;
import java.util.HashSet;
import java.util.Set;

import javax.ejb.EJBException;

import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCCMPFieldMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCCMPFieldPropertyMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCTypeMappingMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCValueClassMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCValuePropertyMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCUserTypeMappingMetaData;
import org.jboss.deployment.DeploymentException;

/**
 * JDBCTypeFactory mapps Java Classes to JDBCType objects.  The main job of
 * this class is to flatten the JDBCValueClassMetaData into columns.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
 * @version $Revision: 1.11.2.10 $
 */
public final class JDBCTypeFactory
{
   //
   // Default CMPFieldStateFactory implementations
   //

   /**
    * This implementation uses field's value as its state.
    */
   public static final CMPFieldStateFactory EQUALS = new CMPFieldStateFactory()
   {
      public Object getFieldState(Object fieldValue)
      {
         return fieldValue;
      }

      public boolean isStateValid(Object state, Object fieldValue)
      {
         return state == null ? fieldValue == null : state.equals(fieldValue);
      }
   };

   /**
    * This implementation will always suppose that the state is invalid unless
    * both states are null.
    */
   private static final CMPFieldStateFactory INVALID_UNLESS_NULL = new CMPFieldStateFactory()
   {
      public Object getFieldState(Object fieldValue)
      {
         return fieldValue;
      }

      public boolean isStateValid(Object state, Object fieldValue)
      {
         return state == null ? fieldValue == null : false;
      }
   };

   /**
    * Field state factory for java.util.Map implementations. The state is
    * a deep copy of the value.
    */
   private static final CMPFieldStateFactory MAP = new CMPFieldStateFactory()
   {
      public Object getFieldState(Object fieldValue)
      {
         return fieldValue == null ? null : new HashMap((Map)fieldValue);
      }

      public boolean isStateValid(Object state, Object fieldValue)
      {
         return (state == null ? fieldValue == null : state.equals(fieldValue));
      }
   };

   /**
    * Field state factory for java.util.List implementations. The state is
    * a deep copy of the value.
    */
   private static final CMPFieldStateFactory LIST = new CMPFieldStateFactory()
   {
      public Object getFieldState(Object fieldValue)
      {
         return fieldValue == null ? null : new ArrayList((List)fieldValue);
      }

      public boolean isStateValid(Object state, Object fieldValue)
      {
         return (state == null ? fieldValue == null : state.equals(fieldValue));
      }
   };

   /**
    * Field state factory for java.util.Set implementations. The state is
    * a deep copy of the value.
    */
   private static final CMPFieldStateFactory SET = new CMPFieldStateFactory()
   {
      public Object getFieldState(Object fieldValue)
      {
         return fieldValue == null ? null : new HashSet((Set)fieldValue);
      }

      public boolean isStateValid(Object state, Object fieldValue)
      {
         return (state == null ? fieldValue == null : state.equals(fieldValue));
      }
   };

   /**
    * Field state factory for arrays. The state is a deep copy of the value.
    */
   private static final CMPFieldStateFactory ARRAY = new CMPFieldStateFactory()
   {
      public Object getFieldState(Object fieldValue)
      {
         Object state = null;
         if(fieldValue != null)
         {
            int length = Array.getLength(fieldValue);
            state = Array.newInstance(fieldValue.getClass().getComponentType(), length);
            System.arraycopy(fieldValue, 0, state, 0, length);
         }
         return state;
      }

      public boolean isStateValid(Object state, Object fieldValue)
      {
         boolean valid;
         if(state == null)
         {
            valid = fieldValue == null;
         }
         else
         {
            if(fieldValue == null)
            {
               valid = false;
            }
            else
            {
               int stateLength = Array.getLength(state);
               if(stateLength != Array.getLength(fieldValue))
               {
                  valid = false;
               }
               else
               {
                  valid = true;
                  for(int i = 0; i < stateLength; ++i)
                  {
                     Object stateEl = Array.get(state, i);
                     Object valueEl = Array.get(fieldValue, i);
                     valid = (stateEl == null ? valueEl == null : stateEl.equals(valueEl));
                     if(!valid)
                     {
                        break;
                     }
                  }
               }
            }
         }
         return valid;
      }
   };

   //
   // Static
   //

   public static final CMPFieldStateFactory getCMPFieldStateFactory(String implClassName, Class clazz)
      throws DeploymentException
   {
      CMPFieldStateFactory stateFactory;
      if(implClassName != null)
      {
         try
         {
            Class implClass = Thread.currentThread().getContextClassLoader().loadClass(implClassName);
            stateFactory = (CMPFieldStateFactory)implClass.newInstance();
         }
         catch(ClassNotFoundException e)
         {
            throw new DeploymentException("Could not load state factory class: " + implClassName);
         }
         catch(Exception e)
         {
            throw new DeploymentException("Failed instantiate state factory: " + implClassName);
         }
      }
      else if(Map.class.isAssignableFrom(clazz))
      {
         stateFactory = MAP;
      }
      else if(List.class.isAssignableFrom(clazz))
      {
         stateFactory = LIST;
      }
      else if(Set.class.isAssignableFrom(clazz))
      {
         stateFactory = SET;
      }
      else if(clazz.isArray())
      {
         stateFactory = ARRAY;
      }
      else if(isDefaultImmutable(clazz))
      {
         stateFactory = EQUALS;
      }
      else
      {
         stateFactory = INVALID_UNLESS_NULL;
      }
      return stateFactory;
   }

   public static final boolean checkDirtyAfterGet(byte checkDirtyAfterGet, Class fieldType)
   {
      boolean result;
      if(checkDirtyAfterGet == JDBCCMPFieldMetaData.CHECK_DIRTY_AFTER_GET_NOT_PRESENT)
      {
         result = !isDefaultImmutable(fieldType);
      }
      else
      {
         result = checkDirtyAfterGet == JDBCCMPFieldMetaData.CHECK_DIRTY_AFTER_GET_TRUE;
      }
      return result;
   }

   private static final boolean isDefaultImmutable(Class clazz)
   {
      boolean result = false;
      if(clazz.isPrimitive()
         || clazz == Boolean.class
         || clazz == Byte.class
         || clazz == Short.class
         || clazz == Integer.class
         || clazz == Long.class
         || clazz == Float.class
         || clazz == Double.class
         || clazz == Character.class
         || clazz == String.class
      )
      {
         result = true;
      }
      return result;
   }

   //
   // Attributes
   //

   // the type mapping to use with the specified database
   private final JDBCTypeMappingMetaData typeMapping;

   // all known complex types by java class type
   private final Map complexTypes = new HashMap();

   /** user types mappings */
   private final Map userTypeMappings;

   public JDBCTypeFactory(JDBCTypeMappingMetaData typeMapping,
                          Collection valueClasses,
                          Map userTypeMappings)
   {
      this.typeMapping = typeMapping;
      this.userTypeMappings = userTypeMappings;

      HashMap valueClassesByType = new HashMap();
      for(Iterator i = valueClasses.iterator(); i.hasNext();)
      {
         JDBCValueClassMetaData valueClass = (JDBCValueClassMetaData)i.next();
         valueClassesByType.put(valueClass.getJavaType(), valueClass);
      }


      // convert the value class meta data to a jdbc complex type
      for(Iterator i = valueClasses.iterator(); i.hasNext();)
      {
         JDBCValueClassMetaData valueClass = (JDBCValueClassMetaData)i.next();
         JDBCTypeComplex type =
            createTypeComplex(valueClass, valueClassesByType);
         complexTypes.put(valueClass.getJavaType(), type);
      }
   }

   public JDBCType getJDBCType(Class javaType)
   {
      if(complexTypes.containsKey(javaType))
      {
         return (JDBCTypeComplex)complexTypes.get(javaType);
      }
      else
      {
         String sqlType = typeMapping.getSqlTypeForJavaType(javaType);
         int jdbcType = typeMapping.getJdbcTypeForJavaType(javaType);
         boolean notNull = javaType.isPrimitive();
         boolean autoIncrement = false;
         return new JDBCTypeSimple(
            null, javaType, jdbcType, sqlType, notNull, autoIncrement, null);
      }
   }

   public JDBCType getJDBCType(JDBCCMPFieldMetaData cmpField) throws DeploymentException
   {
      JDBCType fieldJDBCType;
      final Class fieldType = cmpField.getFieldType();
      if(complexTypes.containsKey(fieldType))
      {
         fieldJDBCType = createTypeComplex(cmpField);
      }
      else
      {
         fieldJDBCType = createTypeSimple(cmpField);
      }
      return fieldJDBCType;
   }

   public int getJDBCTypeForJavaType(Class clazz)
   {
      return typeMapping.getJdbcTypeForJavaType(clazz);
   }

   public JDBCTypeMappingMetaData getTypeMapping()
   {
      return typeMapping;
   }

   private JDBCTypeComplex createTypeComplex(
      JDBCValueClassMetaData valueClass,
      HashMap valueClassesByType)
   {

      // get the properties
      ArrayList propertyList = createComplexProperties(valueClass,
         valueClassesByType, new PropertyStack());

      // transform properties into an array
      JDBCTypeComplexProperty[] properties =
         new JDBCTypeComplexProperty[propertyList.size()];
      properties = (JDBCTypeComplexProperty[])propertyList.toArray(properties);

      return new JDBCTypeComplex(properties, valueClass.getJavaType());
   }

   private JDBCTypeSimple createTypeSimple(JDBCCMPFieldMetaData cmpField) throws DeploymentException
   {
      String columnName = cmpField.getColumnName();
      Class javaType = cmpField.getFieldType();

      int jdbcType;
      String sqlType = cmpField.getSQLType();
      if(sqlType != null)
      {
         jdbcType = cmpField.getJDBCType();
      }
      else
      {
         // get jdbcType and sqlType from typeMapping
         sqlType = typeMapping.getSqlTypeForJavaType(javaType);
         jdbcType = typeMapping.getJdbcTypeForJavaType(javaType);
      }

      boolean notNull = cmpField.isNotNull();
      boolean autoIncrement = cmpField.isAutoIncrement();

      Mapper mapper = null;
      JDBCUserTypeMappingMetaData userTypeMapping =
         (JDBCUserTypeMappingMetaData)userTypeMappings.get(javaType.getName());
      if(userTypeMapping != null)
      {
         String mappedTypeStr = userTypeMapping.getMappedType();
         try
         {
            final ClassLoader contextClassLoader = Thread.currentThread().getContextClassLoader();
            Class mapperClass = contextClassLoader.loadClass(userTypeMapping.getMapper());
            mapper = (Mapper)mapperClass.newInstance();
            javaType = contextClassLoader.loadClass(mappedTypeStr);
            if(cmpField.getSQLType() == null)
            {
               sqlType = typeMapping.getSqlTypeForJavaType(javaType);
               jdbcType = typeMapping.getJdbcTypeForJavaType(javaType);
            }
         }
         catch(ClassNotFoundException e)
         {
            throw new DeploymentException("Class not found for mapper: " + userTypeMapping.getMapper(), e);
         }
         catch(Exception e)
         {
            throw new DeploymentException("Could not instantiate mapper: " + userTypeMapping.getMapper(), e);
         }
      }
      return new JDBCTypeSimple(
         columnName,
         javaType,
         jdbcType,
         sqlType,
         notNull,
         autoIncrement,
         mapper
      );
   }

   private JDBCTypeComplex createTypeComplex(JDBCCMPFieldMetaData cmpField)
   {
      // get the default properties for a field of its type
      JDBCTypeComplex type =
         (JDBCTypeComplex)complexTypes.get(cmpField.getFieldType());
      JDBCTypeComplexProperty[] defaultProperties = type.getProperties();

      // create a map of the overrides based on flat property name
      HashMap overrides = new HashMap();

      for (int i = 0; i < cmpField.getPropertyOverrides().size(); ++i)
      {
         JDBCCMPFieldPropertyMetaData p =
            (JDBCCMPFieldPropertyMetaData)cmpField.getPropertyOverrides().get(i);
         overrides.put(p.getPropertyName(), p);
      }

      // array that will hold the final properites after overrides
      JDBCTypeComplexProperty[] finalProperties =
         new JDBCTypeComplexProperty[defaultProperties.length];

      // override property default values
      for(int i = 0; i < defaultProperties.length; i++)
      {

         // pop off the override, if present
         JDBCCMPFieldPropertyMetaData override;
         override = (JDBCCMPFieldPropertyMetaData)overrides.remove(
            defaultProperties[i].getPropertyName());

         if(override == null)
         {
            finalProperties[i] = defaultProperties[i];
            finalProperties[i] = new JDBCTypeComplexProperty(
               defaultProperties[i],
               cmpField.getColumnName() + "_" +
               defaultProperties[i].getColumnName(),
               defaultProperties[i].getJDBCType(),
               defaultProperties[i].getSQLType(),
               cmpField.isNotNull() || defaultProperties[i].isNotNull());
         }
         else
         {
            // columnName
            String columnName = override.getColumnName();
            if(columnName == null)
            {
               columnName = cmpField.getColumnName() + "_" +
                  defaultProperties[i].getColumnName();
            }

            // sql and jdbc type
            String sqlType = override.getSQLType();
            int jdbcType;
            if(sqlType != null)
            {
               jdbcType = override.getJDBCType();
            }
            else
            {
               sqlType = defaultProperties[i].getSQLType();
               jdbcType = defaultProperties[i].getJDBCType();
            }

            boolean notNull = cmpField.isNotNull() ||
               override.isNotNull() ||
               defaultProperties[i].isNotNull();

            finalProperties[i] = new JDBCTypeComplexProperty(
               defaultProperties[i],
               columnName,
               jdbcType,
               sqlType,
               notNull);
         }
      }

      // did we find all overriden properties
      if(overrides.size() > 0)
      {
         String propertyName = (String)overrides.keySet().iterator().next();
         throw new EJBException("Property " + propertyName + " in field " +
            cmpField.getFieldName() + " is not a property of value object " +
            cmpField.getFieldType().getName());
      }

      // return the new complex type
      return new JDBCTypeComplex(finalProperties, cmpField.getFieldType());
   }

   private ArrayList createComplexProperties(
      JDBCValueClassMetaData valueClass,
      HashMap valueClassesByType,
      PropertyStack propertyStack)
   {

      ArrayList properties = new ArrayList();

      // add the properties each property to the list
      java.util.List valueClassProperties = valueClass.getProperties();
      for (int i = 0; i < valueClassProperties.size(); ++i)
      {
         JDBCValuePropertyMetaData propertyMetaData =
            (JDBCValuePropertyMetaData)valueClassProperties.get(i);
         properties.addAll(createComplexProperties(propertyMetaData,
            valueClassesByType, propertyStack));
      }
      return properties;
   }

   private ArrayList createComplexProperties(
      JDBCValuePropertyMetaData propertyMetaData,
      HashMap valueClassesByType,
      PropertyStack propertyStack)
   {

      // push my data onto the stack
      propertyStack.pushPropertyMetaData(propertyMetaData);

      ArrayList properties = new ArrayList();

      Class javaType = propertyMetaData.getPropertyType();
      if(!valueClassesByType.containsKey(javaType))
      {

         // this property is a simple type
         // which makes this the end of the line for recursion
         String propertyName = propertyStack.getPropertyName();
         String columnName = propertyStack.getColumnName();

         String sqlType = propertyMetaData.getSqlType();
         int jdbcType;
         if(sqlType != null)
         {
            jdbcType = propertyMetaData.getJDBCType();
         }
         else
         {
            // get jdbcType and sqlType from typeMapping
            sqlType = typeMapping.getSqlTypeForJavaType(javaType);
            jdbcType = typeMapping.getJdbcTypeForJavaType(javaType);
         }

         boolean notNull = propertyStack.isNotNull();

         Method[] getters = propertyStack.getGetters();
         Method[] setters = propertyStack.getSetters();

         properties.add(new JDBCTypeComplexProperty(
            propertyName,
            columnName,
            javaType,
            jdbcType,
            sqlType,
            notNull,
            getters,
            setters));

      }
      else
      {

         // this property is a value object, recurse
         JDBCValueClassMetaData valueClass =
            (JDBCValueClassMetaData)valueClassesByType.get(javaType);
         properties.addAll(createComplexProperties(
            valueClass,
            valueClassesByType,
            propertyStack));

      }

      // pop my data, back off
      propertyStack.popPropertyMetaData();

      return properties;
   }

   private static final class PropertyStack
   {
      final ArrayList properties = new ArrayList();
      final ArrayList propertyNames = new ArrayList();
      final ArrayList columnNames = new ArrayList();
      final ArrayList notNulls = new ArrayList();
      final ArrayList getters = new ArrayList();
      final ArrayList setters = new ArrayList();

      public PropertyStack()
      {
      }

      public final void pushPropertyMetaData(
         JDBCValuePropertyMetaData propertyMetaData)
      {

         propertyNames.add(propertyMetaData.getPropertyName());
         columnNames.add(propertyMetaData.getColumnName());
         notNulls.add(new Boolean(propertyMetaData.isNotNull()));
         getters.add(propertyMetaData.getGetter());
         setters.add(propertyMetaData.getSetter());

         if(properties.contains(propertyMetaData))
         {
            throw new EJBException("Circular reference discoverd at " +
               "property: " + getPropertyName());
         }
         properties.add(propertyMetaData);
      }

      public final void popPropertyMetaData()
      {
         propertyNames.remove(propertyNames.size() - 1);
         columnNames.remove(columnNames.size() - 1);
         notNulls.remove(notNulls.size() - 1);
         getters.remove(getters.size() - 1);
         setters.remove(setters.size() - 1);

         properties.remove(properties.size() - 1);
      }

      public final String getPropertyName()
      {
         StringBuffer buf = new StringBuffer();
         for(int i = 0; i < propertyNames.size(); i++)
         {
            if(i > 0)
            {
               buf.append(".");
            }
            buf.append((String)propertyNames.get(i));
         }
         return buf.toString();
      }

      public final String getColumnName()
      {
         StringBuffer buf = new StringBuffer();
         for(int i = 0; i < columnNames.size(); i++)
         {
            if(i > 0)
            {
               buf.append("_");
            }
            buf.append((String)columnNames.get(i));
         }
         return buf.toString();
      }

      public final boolean isNotNull()
      {
         for(int i = 0; i < notNulls.size(); i++)
         {
            if(((Boolean)notNulls.get(i)).booleanValue())
            {
               return true;
            }
         }
         return false;
      }

      public final Method[] getGetters()
      {
         return (Method[])getters.toArray(new Method[getters.size()]);
      }

      public final Method[] getSetters()
      {
         return (Method[])setters.toArray(new Method[setters.size()]);
      }
   }
}
