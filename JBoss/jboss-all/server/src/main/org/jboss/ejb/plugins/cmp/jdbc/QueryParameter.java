package org.jboss.ejb.plugins.cmp.jdbc;

import java.lang.reflect.Method;
import java.sql.PreparedStatement;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.StringTokenizer;
import javax.ejb.EJBException;
import javax.ejb.EJBLocalObject;
import javax.ejb.EJBObject;

import org.jboss.ejb.plugins.cmp.ejbql.Catalog;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMPFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCEntityBridge;

import org.jboss.logging.Logger;

public class QueryParameter {
   public static List createParameters(
         int argNum, 
         JDBCCMPFieldBridge field) {

      List parameters;

      JDBCType type = field.getJDBCType();
      if(type instanceof JDBCTypeComplex) {
         JDBCTypeComplexProperty[] props = 
               ((JDBCTypeComplex)type).getProperties();
         
         parameters = new ArrayList(props.length);
         
         for(int i=0; i<props.length; i++) {
            QueryParameter param = new QueryParameter(
                     argNum,
                     false,
                     null,
                     props[i],
                     props[i].getJDBCType());
            parameters.add(param);
         }
      } else {
         parameters = new ArrayList(1);

         QueryParameter param = new QueryParameter(
                  argNum,
                  false,
                  null,
                  null,
                  type.getJDBCTypes()[0]);
         parameters.add(param);
      }
      return parameters;
   } 

   public static List createParameters(
         int argNum, 
         JDBCEntityBridge entity) {

      List parameters = new ArrayList();

      List pkFields = entity.getPrimaryKeyFields();
      for(Iterator iter = pkFields.iterator(); iter.hasNext();) {
         JDBCCMPFieldBridge pkField = (JDBCCMPFieldBridge)iter.next();

         JDBCType type = pkField.getJDBCType();
         if(type instanceof JDBCTypeComplex) {
            JDBCTypeComplexProperty[] props = 
                  ((JDBCTypeComplex)type).getProperties();
            for(int j=0; j<props.length; j++) {
               QueryParameter param = new QueryParameter(
                        argNum,
                        false,
                        pkField,
                        props[j],
                        props[j].getJDBCType());
               parameters.add(param);
            }
         } else {
            QueryParameter param = new QueryParameter(
                     argNum,
                     false,
                     pkField,
                     null,
                     type.getJDBCTypes()[0]);
            parameters.add(param);
         }
      }
      return parameters;
   } 
   
   public static List createPrimaryKeyParameters(
         int argNum, 
         JDBCEntityBridge entity) {

      List parameters = new ArrayList();

      List pkFields = entity.getPrimaryKeyFields();
      for(Iterator iter = pkFields.iterator(); iter.hasNext();) {
         JDBCCMPFieldBridge pkField = (JDBCCMPFieldBridge)iter.next();

         JDBCType type = pkField.getJDBCType();
         if(type instanceof JDBCTypeComplex) {
            JDBCTypeComplexProperty[] props = 
                  ((JDBCTypeComplex)type).getProperties();
            for(int j=0; j<props.length; j++) {
               QueryParameter param = new QueryParameter(
                        argNum,
                        true,
                        pkField,
                        props[j],
                        props[j].getJDBCType());
               parameters.add(param);
            }
         } else {
            QueryParameter param = new QueryParameter(
                     argNum,
                     true,
                     pkField,
                     null,
                     type.getJDBCTypes()[0]);
            parameters.add(param);
         }
      }
      return parameters;
   }

   private int argNum;
   private boolean isPrimaryKeyParameter;
   private JDBCCMPFieldBridge field;
   private JDBCTypeComplexProperty property;
   private String parameterString;
   
   private int jdbcType;

   public QueryParameter(
         JDBCStoreManager manager, 
         Method method, 
         String parameterString) {

      // Method parameter will never be a primary key object, but always
      // a complete entity.
      this.isPrimaryKeyParameter = false;
      
      this.parameterString = parameterString;

      if(parameterString == null || parameterString.length() == 0) {
         throw new IllegalArgumentException("Parameter string is empty");
      }

      StringTokenizer tok = new StringTokenizer(parameterString, ".");

      // get the argument number
      try {
         argNum = Integer.parseInt(tok.nextToken());
      } catch(NumberFormatException e) {
         throw new IllegalArgumentException("The parameter must begin with " +
               "a number");
      }

      // get the argument type
      if(argNum > method.getParameterTypes().length) {
         throw new IllegalArgumentException("The parameter index is " + argNum +
               " but the query method only has " +
               method.getParameterTypes().length + "parameter(s)");
      }
      Class argType = method.getParameterTypes()[argNum];

      // get the jdbc type object
      JDBCType type;

      // if this is an entity parameter
      if(EJBObject.class.isAssignableFrom(argType) || 
            EJBLocalObject.class.isAssignableFrom(argType)) {

         // get the field name
         // check more tokens
         if(!tok.hasMoreTokens()) {
            throw new IllegalArgumentException("When the parameter is an " +
                  "ejb a field name must be supplied.");
         }
         String fieldName = tok.nextToken();

         // get the field from the entity
         field = getCMPField(manager, argType, fieldName);
         if(!field.isPrimaryKeyMember()) {
            throw new IllegalArgumentException("The specified field must be " +
                  "a primay key field");
         }

         // get the jdbc type object
         type = field.getJDBCType();
      } else {
         // get jdbc type from type manager
         type = manager.getJDBCTypeFactory().getJDBCType(argType);
      }

      if(type instanceof JDBCTypeSimple) {
         if(tok.hasMoreTokens()){
            throw new IllegalArgumentException("Parameter is NOT a known " +
                  "dependent value class, so a properties cannot supplied.");
         }
         jdbcType = type.getJDBCTypes()[0];
      } else {
         if(!tok.hasMoreTokens()){
            throw new IllegalArgumentException("Parmeter is a known " +
                  "dependent value class, so a property must be supplied");
         }

         // build the propertyName
         StringBuffer propertyName = new StringBuffer(parameterString.length());
         propertyName.append(tok.nextToken());
         while(tok.hasMoreTokens()) {
            propertyName.append(".").append(tok.nextToken());
         }
            
         property = ((JDBCTypeComplex)type).getProperty(
               propertyName.toString());

         jdbcType = property.getJDBCType();
      }
   }

   public QueryParameter(
         int argNum, 
         boolean isPrimaryKeyParameter,
         JDBCCMPFieldBridge field,
         JDBCTypeComplexProperty property,
         int jdbcType) {

      this.argNum = argNum;
      this.isPrimaryKeyParameter = isPrimaryKeyParameter;
      this.field = field;
      this.property = property;
      this.jdbcType = jdbcType;

      StringBuffer parameterBuf = new StringBuffer();
      parameterBuf.append(argNum);
      if(field != null) {
         parameterBuf.append(".").append(field.getFieldName());
      }
      if(property != null) {
         parameterBuf.append(".").append(property.getPropertyName());
      }
      parameterString = parameterBuf.toString();
   }

   /**
    * Gets the dotted parameter string for this parameter.
    */
   public String getParameterString() {
      return parameterString;
   }
   
   public void set(Logger log, PreparedStatement ps, int index, Object[] args) 
         throws Exception {

      Object arg = args[argNum];
      if(field != null) {
         if(!isPrimaryKeyParameter) {
            if(arg instanceof EJBObject) {
               arg = ((EJBObject)arg).getPrimaryKey();
            } else if(arg instanceof EJBLocalObject) {
               arg = ((EJBLocalObject)arg).getPrimaryKey();
            } else {
               throw new IllegalArgumentException("Expected an instanc of " +
                     "EJBObject or EJBLocalObject, but got an instance of " + 
                     arg.getClass().getName());
            }
         }
         arg = field.getPrimaryKeyValue(arg);
      }
      if(property != null) {
         arg = property.getColumnValue(arg);
      }
      JDBCUtil.setParameter(log, ps, index, jdbcType, arg);
   }

   private JDBCCMPFieldBridge getCMPField(
         JDBCStoreManager manager, 
         Class intf,
         String fieldName) {

      Catalog catalog = (Catalog)manager.getApplicationData("CATALOG");
      JDBCEntityBridge entityBridge = 
            (JDBCEntityBridge)catalog.getEntityByInterface(intf);
      if(entityBridge == null) {
         throw new IllegalArgumentException("Entity not found in application " +
               "catalog with interface=" + intf.getName());
      }

      JDBCCMPFieldBridge cmpField = entityBridge.getCMPFieldByName(fieldName);
      if(cmpField == null) {
         throw new IllegalArgumentException("cmpField not found:" + 
               " cmpFieldName=" + fieldName + 
               " entityName=" + entityBridge.getEntityName());
      }
      return cmpField;
   } 

   public String toString() {
      return parameterString;
   }
}
