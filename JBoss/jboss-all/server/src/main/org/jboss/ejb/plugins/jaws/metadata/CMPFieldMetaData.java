/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.jaws.metadata;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.sql.Types;
import java.util.ArrayList;

import org.w3c.dom.Element;

import org.jboss.deployment.DeploymentException;

import org.jboss.metadata.XmlLoadable;
import org.jboss.metadata.MetaData;

import java.util.*;

import org.jboss.logging.Logger;

/**
 * This class holds all the information jaws needs to know about a CMP field.
 * It loads its data from standardjaws.xml and jaws.xml
 *
 * @see <related>
 * @author <a href="sebastien.alborini@m4x.org">Sebastien Alborini</a>
 * @author <a href="mailto:dirk@jboss.de">Dirk Zimmermann</a>
 * @author <a href="mailto:vincent.harcq@hubmethods.com">Vincent Harcq</a>
 * @author <a href="mailto:david_jencks@earthlink.net">David Jencks</a>
 * @version $Revision: 1.14.4.1 $
 *
 * Revison:
 * 20010621 danch: merged patch from David Jenks - null constraint on columns.
 */
public class CMPFieldMetaData extends MetaData implements XmlLoadable
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------
   protected static Logger log = Logger.getLogger(CMPFieldMetaData.class);

   /** The entity this field belongs to. */
   private JawsEntityMetaData jawsEntity;

   /** Name of the field. */
   private String name;

   /** The actual field in the bean implementation. */
   private Field field;

   /** The jdbc type (see java.sql.Types), used in PreparedStatement.setParameter . */
   private int jdbcType;

   /** True if jdbcType has been initialized. */
   private boolean validJdbcType;

   /** The sql type, used for table creation. */
   private String sqlType;

   /** The column name in the table. */
   private String columnName;

   /** For table creation, whether to include not null constraint on column. */
   private boolean nullable = true;

   private boolean isAPrimaryKeyField;

   /** We need this for nested field retrieval. */
   private String ejbClassName;

   /**
    * We need this for nested fields. We could compute it from ejbClassName on the fly,
    * but it's faster to set it once and cache it.
    */
   private Class ejbClass;

   /**
    * Is true for fields like "data.categoryPK".
    */
   private boolean isNested;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   public CMPFieldMetaData(String name, JawsEntityMetaData jawsEntity)
      throws DeploymentException
   {
      this.name = name;
      this.jawsEntity = jawsEntity;

      // save the class name for nested fields
      ejbClassName = jawsEntity.getEntity().getEjbClass();
//-re- huh? twice is safer?      ejbClassName = jawsEntity.getEntity().getEjbClass();

      try {
         // save the class for nested fields
         ejbClass = jawsEntity.getJawsApplication().getClassLoader().loadClass(ejbClassName);
         field = ejbClass.getField(name);
      } catch (ClassNotFoundException e) {
         throw new DeploymentException("ejb class not found: " + ejbClassName);
      } catch (NoSuchFieldException e) {
         // we can't throw an Exception here, because we could have a nested field
         checkField();
      }

      // default, may be overridden by importXml
      columnName = getLastComponent(name);

      // cannot set defaults for jdbctype/sqltype, type mappings are not loaded yet.
   }


   // Public --------------------------------------------------------

   public String getName() { return name; }

   public Field getField() { return field; }

   public int getJDBCType() {
      if (! validJdbcType) {
         // set the default
         if (field!=null)
         {
            jdbcType = jawsEntity.getJawsApplication().getTypeMapping().getJdbcTypeForJavaType(field.getType());
         }
         else
         {
             try
             {
                jdbcType = jawsEntity.getJawsApplication().getTypeMapping().getJdbcTypeForJavaType(ValueObjectHelper.getNestedFieldType(ejbClass,name));
             }
             catch(NoSuchMethodException e)
             {
                 log.warn("Nested field \""+name+"\" in class "+ejbClass.getName()+" does not have a get method");
             }
         }
         validJdbcType = true;
      }
      return jdbcType;
   }

   public String getSQLType() {
      if (sqlType == null) {
         // set the default
         if (field!=null)
         {
            sqlType = jawsEntity.getJawsApplication().getTypeMapping().getSqlTypeForJavaType(field.getType());
         }
         else
         {
            try
            {
               sqlType = jawsEntity.getJawsApplication().getTypeMapping().getSqlTypeForJavaType(ValueObjectHelper.getNestedFieldType(ejbClass,name));
            }
            catch(NoSuchMethodException e)
            {
               log.warn("Nested field \""+name+"\" in class "+ejbClass.getName()+" does not have a get method");
            }
         }
      }
      return sqlType;
   }

   public String getNullable() {
      if (nullable)
      {
         return "";
      }
      else
      {
         return " NOT NULL";
      }
   }

   public String getColumnName() { return columnName; }

   public boolean isEJBReference() { return jdbcType == Types.REF; }

   public boolean isAPrimaryKeyField() { return isAPrimaryKeyField; }

   public JawsEntityMetaData getJawsEntity() { return jawsEntity; }

   /**
    * Returns the last component of a composite fieldName. E.g., for "data.categoryPK" it
    * will return "categoryPK".
    */
   public static String getLastComponent(String name) {
      String fieldName = name;
      StringTokenizer st = new StringTokenizer(name, ".");
      while(st.hasMoreTokens()) {
         fieldName = st.nextToken();
      }
      return fieldName;
   }

   /**
    * Returns the first component of a composite fieldName. E.g., for "data.categoryPK" it
    * will return "data".
    */
   public static String getFirstComponent(String name) {
      String fieldName;
      StringTokenizer st = new StringTokenizer(name, ".");
      if (st.hasMoreTokens())
      {
         fieldName = st.nextToken();
      }
      else
      {
         fieldName = null;
      }
      return fieldName;
   }

   /**
    * Detects the actual field of a nested field and sets field accordingly.
    * If field doesn't exist, throws a DeploymentException.
    */
   private void checkField() throws DeploymentException {
      try
      {
         field = verifyNestedField();
      }
      catch(DeploymentException e)
      {
         // try it again, but debug Class before :))
         debugClass(ejbClass);
         field = verifyNestedField();
         log.warn("!!! using buggy hotspot, try to upgrade ... !!!");
      }
   }

   /**
    * Traverses and verifies a nested field, so that every field given in jaws.xml
    * exists in the Bean.
    */
   private Field verifyNestedField() throws DeploymentException {
      String fieldName = null;
      Field tmpField = null;
      Class tmpClass = ejbClass;
      StringTokenizer st = new StringTokenizer(name, ".");
      boolean debug = log.isDebugEnabled();

      if (st.countTokens() > 1)
      {
         isNested = true;
      }

      while(st.hasMoreTokens())
      {
         fieldName = st.nextToken();
         try
         {
            //debugClass(tmpClass);
            tmpField = tmpClass.getField(fieldName);
            tmpClass = tmpField.getType();
            if (debug)
               log.debug("(Dependant Object) "+tmpField.getName());
         }
         catch (NoSuchFieldException e)
         {
            // we can have a private attribute, then we will use fieldName
            // to find the get/set methods, but still have to set jdbcType/SQLType
            // but can not yet do it sowe have to set field to null so that
            // getJDBCType will not use the parent Field to find the types
            field = null;
            return null;
         }
      }
      return tmpField;
   }

   /**
    * We don't rely on the field alone for getting the type since we support nested field
    * like 'data.categoryPK'.
    */
   public Class getFieldType() {
      if (field != null)
      {
         // The default case as it always was :)
         return field.getType();
      }

      // We obviously have a nested field (or an erroneous one)
      Field tmpField = null;
      Class tmpClass = ejbClass;
      String fieldName = null;
      StringTokenizer st = new StringTokenizer(name, ".");
      while(st.hasMoreTokens())
      {
         fieldName = st.nextToken();
         try
         {
            tmpField = tmpClass.getField(fieldName);
            tmpClass = tmpField.getType();
         }
         catch (NoSuchFieldException e)
         {
            // We have a nested Field
            try
            {
               return ValueObjectHelper.getNestedFieldType(ejbClass,name);
            }
            catch (NoSuchMethodException ne)
            {
               log.warn("Nested field "+fieldName+" does not have a get method on "+ejbClass.getName());
               return null;
            }
         }
      }
      return tmpField.getType();
   }

   /**
    * Is used mainly for nested fields. Sets the value of a nested field.
    */
   public void set(Object instance, Object value) {
      Field tmpField = null;
      String fieldName = null;
      Object currentObject = instance;
      Object oldObject;
      StringTokenizer st = new StringTokenizer(name, ".");
      //log.debug("set on cmp-field "+name+"="+value);
      // First we instanciate nested objects if they do not already exist
      int i=1;
      int tot=st.countTokens();
      while (st.hasMoreTokens() && (i < tot) )
      {
         i++;
         fieldName = st.nextToken();
         //log.debug("initialize "+fieldName+ " on "+currentObject.getClass());
         oldObject = currentObject;
         try
         {
            tmpField = currentObject.getClass().getField(fieldName);
            currentObject = tmpField.get(currentObject);
            // On our path, we have to instantiate every intermediate object
            if (currentObject == null)
            {
               currentObject = tmpField.getType().newInstance();
               tmpField.set(oldObject, currentObject);
            }
         }
         catch (NoSuchFieldException ne)
         {
            try
            {
               currentObject = ValueObjectHelper.getValue(currentObject,fieldName);
               if (currentObject == null)
               {
                  currentObject = ValueObjectHelper.getNestedFieldType(oldObject.getClass(),fieldName).newInstance();
                  ValueObjectHelper.setValue(oldObject,fieldName,currentObject);
               }
            }
            catch (NoSuchMethodException e)
            {
               log.warn("set method not found for " + fieldName + " on " + oldObject.getClass().getName());
            }
            catch (InvocationTargetException e)
            {
               log.warn("set method not invocable " + fieldName + " on " + currentObject.getClass().getName());
            }
            catch (IllegalAccessException e)
            {
               log.warn("!!! Deployment Failure !!!" + e);
            }
            catch (InstantiationException e)
            {
               log.warn("could not instantiate " + tmpField);
            }
         }
         catch (IllegalAccessException e)
         {
            log.warn("!!! Deployment Failure !!!" + e);
         }
         catch (InstantiationException e)
         {
            log.warn("could not instantiate " + tmpField);
         }
         catch (Exception e)
         {
            log.warn("Exception " + e);
         }
      }
      //log.debug("initialization of nested objects done for "+name);
      // Now we set the value of the last component into the created object
      try
      {
         try
         {
            Field dataField = currentObject.getClass().getField(getLastComponent(name));
            dataField.set(currentObject, value);
         }
         catch (NoSuchFieldException nse)
         {
            //log.debug("set on "+getLastComponent(name)+ " on "+currentObject.getClass()+ "="+value);
            ValueObjectHelper.setValue(currentObject,getLastComponent(name),value);
         }
      }
      catch (IllegalAccessException e)
      {
         log.warn("!!! Deployment Failure !!!" + e);
      }
      catch (InvocationTargetException e)
      {
         log.warn("set method not invocable " + getLastComponent(name) + " on " + currentObject.getClass().getName());
      }
      catch (NoSuchMethodException e)
      {
         log.warn("set method not found for " + getLastComponent(name) + " on " + currentObject.getClass().getName());
      }
   }

   /**
    * Returns the value of this field.
    */
   public Object getValue(Object instance) {
      String fieldName;
      Object currentObject = instance;
      Field currentField;
      //Object currentValue = null;

      try
      {
         if (!isNested())
         {
            return getField().get(instance);
         }
         else
         {
            StringTokenizer st = new StringTokenizer(name, ".");
            while(st.hasMoreTokens())
            {
               fieldName = st.nextToken();
               if (currentObject == null) return null;
               try
               {
                  currentField = currentObject.getClass().getField(fieldName);
                  currentObject = currentField.get(currentObject);
               }
               catch(NoSuchFieldException e){
                  currentField = null;
                  currentObject = ValueObjectHelper.getValue(currentObject,fieldName);
               }
            }
            return currentObject;
         }
      }
      catch (IllegalAccessException e)
      {
         // We have already checked the presence of this field in the constructor,
         // so there is no need to throw an exception here.
         log.warn("!!! CMPFieldMetaData.getValue() ERROR !!! " + e);
      }
      catch (InvocationTargetException e)
      {
         log.warn("!!! CMPFieldMetaData.getValue() ERROR !!! " + e);
      }
      catch (NoSuchMethodException e)
      {
         log.warn("!!! CMPFieldMetaData.getValue() ERROR !!! " + e);
      }
      return null;
   }

   public boolean isNested() {
      return isNested;
   }

   // XmlLoadable implementation ------------------------------------
   public void importXml(Element element)
      throws DeploymentException
   {

      // column name
      String columnStr = getElementContent(getOptionalChild(element, "column-name"));

      // For Netsted Properties, we will have a column name blank which means
      // the CMPField need to be removed.  It will be reconstrcut and decompose
      // by Jaws when needed
      if (columnStr != null)
      {
         columnName = columnStr;
      }

      // jdbc type
      String jdbcStr = getElementContent(getOptionalChild(element, "jdbc-type"));

      if (jdbcStr != null)
      {
         jdbcType = MappingMetaData.getJdbcTypeFromName(jdbcStr);
         validJdbcType = true;

         sqlType = getElementContent(getUniqueChild(element, "sql-type"));

         String nullableStr = getElementContent(getOptionalChild(element, "nullable"));

         if (nullableStr != null)
         {
             nullable = Boolean.valueOf(nullableStr).booleanValue();
         }

      }

   }


   // Package protected ---------------------------------------------

   void setPrimary() {
      isAPrimaryKeyField = true;
   }

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   /**
    * Workaround for certain Hotspot problems. Just traverse all the fields
    * in the Class, so Hotspot won't optimize to bad ...
    */
   private void debugClass(Class debugClass) {
      Field[] fields = debugClass.getFields();
      for (int i = 0; i < fields.length; ++i) {
      }
   }

   // Inner classes -------------------------------------------------
}
