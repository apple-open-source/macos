/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc.metadata;

import java.util.HashMap;
import java.util.Iterator;

import org.jboss.deployment.DeploymentException;
import org.jboss.metadata.MetaData;
import org.w3c.dom.Element;

/**
 * Imutable class which holds a map between Java Classes and JDBCMappingMetaData.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="sebastien.alborini@m4x.org">Sebastien Alborini</a>
 * @author <a href="mailto:loubyansky@ua.fm">Alex Loubyansky</a>
 * @version $Revision: 1.13.2.6 $
 */
public final class JDBCTypeMappingMetaData
{
   private static final String[] PRIMITIVES = {
      "boolean", "byte", "char", "short", "int", "long", "float", "double"
   };

   private static final String[] PRIMITIVE_CLASSES = {
      "java.lang.Boolean", "java.lang.Byte", "java.lang.Character",
      "java.lang.Short", "java.lang.Integer", "java.lang.Long",
      "java.lang.Float", "java.lang.Double"
   };

   public static final String CONCAT = "concat";
   public static final String SUBSTRING = "substring";
   public static final String LCASE = "lcase";
   public static final String UCASE = "ucase";
   public static final String LENGTH = "length";
   public static final String LOCATE = "locate";
   public static final String ABS = "abs";
   public static final String SQRT = "sqrt";
   public static final String COUNT = "count";

   public static JDBCFunctionMappingMetaData MAX_FUNC;
   public static JDBCFunctionMappingMetaData MIN_FUNC;
   public static JDBCFunctionMappingMetaData AVG_FUNC;
   public static JDBCFunctionMappingMetaData SUM_FUNC;

   static
   {
      try
      {
         MAX_FUNC = new JDBCFunctionMappingMetaData("max", "max(?1)");
         MIN_FUNC = new JDBCFunctionMappingMetaData("min", "min(?1)");
         AVG_FUNC = new JDBCFunctionMappingMetaData("avg", "avg(?1)");
         SUM_FUNC = new JDBCFunctionMappingMetaData("sum", "sum(?1)");
      }
      catch(DeploymentException e)
      {
         throw new IllegalStateException(e.getMessage());
      }
   }

   private final String name;

   private final HashMap mappings = new HashMap();

   private final HashMap functionMappings = new HashMap();

   private final String aliasHeaderPrefix;
   private final String aliasHeaderSuffix;
   private final int aliasMaxLength;

   private final boolean subquerySupported;

   private final String trueMapping;
   private final String falseMapping;

   private JDBCFunctionMappingMetaData rowLocking = null;
   private JDBCFunctionMappingMetaData fkConstraint = null;
   private JDBCFunctionMappingMetaData pkConstraint = null;
   private JDBCFunctionMappingMetaData autoIncrement = null;

   /**
    * Constructs a mapping with the data contained in the type-mapping xml
    * element from a jbosscmp-jdbc xml file.
    *
    * @param element the xml Element which contains the metadata about
    *       this type mapping
    * @throws DeploymentException if the xml element is not semantically correct
    */
   public JDBCTypeMappingMetaData(Element element) throws DeploymentException
   {
      // get the name of this type-mapping
      name = MetaData.getUniqueChildContent(element, "name");

      // row-locking (i.e., select for update)
      String rowLockingSQL = MetaData.getUniqueChildContent(element, "row-locking-template");
      if(rowLockingSQL != null && !rowLockingSQL.trim().equals(""))
      {
         rowLocking = new JDBCFunctionMappingMetaData("row-locking", rowLockingSQL);
      }

      // pk constraint
      String pkConstraintSQL = MetaData.getUniqueChildContent(element, "pk-constraint-template");
      if(pkConstraintSQL != null && !pkConstraintSQL.trim().equals(""))
      {
         pkConstraint = new JDBCFunctionMappingMetaData("pk-constraint", pkConstraintSQL);
      }

      // fk constraint
      String fkConstraintSQL = MetaData.getUniqueChildContent(element, "fk-constraint-template");
      if(fkConstraintSQL != null && !fkConstraintSQL.trim().equals(""))
      {
         fkConstraint = new JDBCFunctionMappingMetaData("fk-constraint", fkConstraintSQL);
      }

      // auto increment
      // WARN: it's optional
      String autoIncrementSQL = MetaData.getOptionalChildContent(element, "auto-increment-template");
      if(autoIncrementSQL != null && !autoIncrementSQL.trim().equals(""))
      {
         autoIncrement = new JDBCFunctionMappingMetaData("auto-increment", autoIncrementSQL);
      }

      // get the mappings
      Iterator iterator = MetaData.getChildrenByTagName(element, "mapping");
      while(iterator.hasNext())
      {
         Element mappingElement = (Element)iterator.next();
         JDBCMappingMetaData mapping = new JDBCMappingMetaData(mappingElement);
         mappings.put(mapping.getJavaType(), mapping);
      }

      addDefaultFunctionMapping();

      // get the mappings
      Iterator functions = MetaData.getChildrenByTagName(element, "function-mapping");
      while(functions.hasNext())
      {
         Element mappingElement = (Element)functions.next();
         JDBCFunctionMappingMetaData functionMapping = new JDBCFunctionMappingMetaData(mappingElement);
         functionMappings.put(functionMapping.getFunctionName().toLowerCase(), functionMapping);
      }

      aliasHeaderPrefix = MetaData.getUniqueChildContent(element, "alias-header-prefix");

      aliasHeaderSuffix = MetaData.getUniqueChildContent(element, "alias-header-suffix");

      String aliasMaxLengthString = MetaData.getUniqueChildContent(element, "alias-max-length");
      try
      {
         aliasMaxLength = Integer.parseInt(aliasMaxLengthString);
      }
      catch(NumberFormatException e)
      {
         throw new DeploymentException("Invalid number format in " +
            "alias-max-length " + aliasMaxLengthString + "': " + e);
      }

      String subquerySupportedStr = MetaData.getUniqueChildContent(element, "subquery-supported");
      subquerySupported = Boolean.valueOf(subquerySupportedStr).booleanValue();
      trueMapping = MetaData.getUniqueChildContent(element, "true-mapping");
      falseMapping = MetaData.getUniqueChildContent(element, "false-mapping");
   }

   /**
    * Gets the name of this mapping. The mapping name used to differentiate this
    * mapping from other mappings and the mapping the application used is
    * retrieved by name.
    * @return the name of this mapping.
    */
   public String getName()
   {
      return name;
   }

   /**
    * Gets the prefix for that is used when generating an alias header.  An
    * alias header is prepended to a generated table alias to prevent name
    * collisions. An alias header is constructed as folows:
    * aliasHeaderPrefix + int_counter + aliasHeaderSuffix
    *
    * @return the prefix for alias headers
    */
   public String getAliasHeaderPrefix()
   {
      return aliasHeaderPrefix;
   }

   /**
    * Gets the suffix for that is used when generating an alias header.  An
    * alias header is prepended to a generated table alias to prevent name
    * collisions. An alias header is constructed as folows:
    * aliasHeaderPrefix + int_counter + aliasHeaderSuffix
    *
    * @return the suffix for alias headers
    */
   public String getAliasHeaderSuffix()
   {
      return aliasHeaderSuffix;
   }

   /**
    * Gets maximum length of a table alias.
    * An alias is constructed as folows: aliasHeader + ejb_ql_identifier_path
    * @return the maximum length that a table alias can be
    */
   public int getAliasMaxLength()
   {
      return aliasMaxLength;
   }

   /**
    * Does this type mapping support subqueries?
    */
   public boolean isSubquerySupported()
   {
      return subquerySupported;
   }

   /**
    * Gets the value to which the boolean true value in EJB-QL will be mapped.
    */
   public String getTrueMapping()
   {
      return trueMapping;
   }

   /**
    * Gets the value to which the boolean false value in EJB-QL will be mapped.
    */
   public String getFalseMapping()
   {
      return falseMapping;
   }

   /**
    * Gets the jdbc type which this class has mapped to the specified java
    * class. The jdbc type is used to retrieve data from a result set and to
    * set parameters in a prepared statement.
    *
    * @param type the Class for which the jdbc type will be returned
    * @return the jdbc type which is mapped to the type
    */
   public int getJdbcTypeForJavaType(Class type)
   {
      String javaType = type.getName();

      // Check primitive first
      for(int i = 0; i < PRIMITIVES.length; i++)
      {
         if(javaType.equals(PRIMITIVES[i]))
         {
            // Translate into class
            javaType = PRIMITIVE_CLASSES[i];
            break;
         }
      }

      // Check other types
      JDBCMappingMetaData mapping = (JDBCMappingMetaData)mappings.get(javaType);

      // if not found, return mapping for java.lang.object
      if(mapping == null)
      {
         mapping = (JDBCMappingMetaData)mappings.get("java.lang.Object");
      }

      return mapping.getJdbcType();
   }

   /**
    * Gets the sql type which this class has mapped to the java class. The sql
    * type is the sql column data type, and is used in CREATE TABLE statements.
    *
    * @param type the Class for which the sql type will be returned
    * @return the sql type which is mapped to the type
    */
   public String getSqlTypeForJavaType(Class type)
   {
      String javaType = type.getName();

      // if the type is a primitive convert it to it's wrapper class
      for(int i = 0; i < PRIMITIVES.length; i++)
      {
         if(javaType.equals(PRIMITIVES[i]))
         {
            // Translate into class
            javaType = PRIMITIVE_CLASSES[i];
            break;
         }
      }

      // Check other types
      JDBCMappingMetaData mapping = (JDBCMappingMetaData)mappings.get(javaType);

      // if not found, return mapping for java.lang.object
      if(mapping == null)
      {
         mapping = (JDBCMappingMetaData)mappings.get("java.lang.Object");
      }

      return mapping.getSqlType();
   }

   public JDBCFunctionMappingMetaData getFunctionMapping(String name)
   {
      JDBCFunctionMappingMetaData funcMapping = (JDBCFunctionMappingMetaData)functionMappings.get(name.toLowerCase());
      if(funcMapping == null)
         throw new IllegalStateException("Function " + name + " is not defined for " + this.name);
      return funcMapping;
   }


   /**
    * Returns rowLocking SQL template.
    */
   public JDBCFunctionMappingMetaData getRowLockingTemplate()
   {
      return rowLocking;
   }

   /**
    * Returns pk constraint SQL template.
    */
   public JDBCFunctionMappingMetaData getPkConstraintTemplate()
   {
      return pkConstraint;
   }

   /**
    * Returns fk constraint SQL template.
    */
   public JDBCFunctionMappingMetaData getFkConstraintTemplate()
   {
      return fkConstraint;
   }

   /**
    * Returns auto increment SQL template.
    */
   public JDBCFunctionMappingMetaData getAutoIncrementTemplate()
   {
      return autoIncrement;
   }

   private void addDefaultFunctionMapping()
   {
      JDBCFunctionMappingMetaData function;

      // concat
      function = new JDBCFunctionMappingMetaData("concat",
         new String[]{
            "{fn concat(",
            ", ",
            ")}"
         },
         new int[]{0, 1});
      functionMappings.put(function.getFunctionName().toLowerCase(), function);

      // substring
      function = new JDBCFunctionMappingMetaData("substring",
         new String[]{
            "{fn substring(",
            ", ",
            ", ",
            ")}"
         },
         new int[]{0, 1, 2});
      functionMappings.put(function.getFunctionName().toLowerCase(), function);

      // lcase
      function = new JDBCFunctionMappingMetaData("lcase",
         new String[]{
            "{fn lcase(",
            ")}"
         },
         new int[]{0});
      functionMappings.put(function.getFunctionName().toLowerCase(), function);

      // ucase
      function = new JDBCFunctionMappingMetaData("ucase",
         new String[]{
            "{fn ucase(",
            ")}"
         },
         new int[]{0});
      functionMappings.put(function.getFunctionName().toLowerCase(), function);

      // length
      function = new JDBCFunctionMappingMetaData("length",
         new String[]{
            "{fn length(",
            ")}"
         },
         new int[]{0});
      functionMappings.put(function.getFunctionName().toLowerCase(), function);

      // locate
      function = new JDBCFunctionMappingMetaData("locate",
         new String[]{
            "{fn locate(",
            ", ",
            ", ",
            ")}"
         },
         new int[]{0, 1, 2});
      functionMappings.put(function.getFunctionName().toLowerCase(), function);

      // abs
      function = new JDBCFunctionMappingMetaData("abs",
         new String[]{
            "{fn abs(",
            ")}"
         },
         new int[]{0});
      functionMappings.put(function.getFunctionName().toLowerCase(), function);

      // sqrt
      function = new JDBCFunctionMappingMetaData("sqrt",
         new String[]{
            "{fn sqrt(",
            ")}"
         },
         new int[]{0});
      functionMappings.put(function.getFunctionName().toLowerCase(), function);
   }
}
