/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.cmp.jdbc;

import java.sql.Connection;
import java.sql.DatabaseMetaData;
import javax.sql.DataSource;
import java.sql.SQLException;
import java.sql.ResultSet;
import java.util.zip.CRC32;

import org.jboss.deployment.DeploymentException;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCEntityBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMPFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMRFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCFieldBridge;

import java.util.Vector;

/**
 * SQLUtil helps with building sql statements.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 * @version $Revision: 1.12.4.14 $
 */
public final class SQLUtil
{
   public static final String EMPTY_STRING = "";
   public static final String INSERT_INTO = "INSERT INTO ";
   public static final String VALUES = " VALUES ";
   public static final String SELECT = "SELECT ";
   public static final String DISTINCT = "DISTINCT ";
   public static final String FROM = " FROM ";
   public static final String WHERE = " WHERE ";
   public static final String ORDERBY = " ORDER BY ";
   public static final String DELETE_FROM = "DELETE FROM ";
   public static final String AND = " AND ";
   public static final String OR = " OR ";
   public static final String NOT = " NOT ";
   public static final String EXISTS = "EXISTS ";
   public static final String COMMA = ", ";
   public static final String LEFT_JOIN = " LEFT JOIN ";
   public static final String LEFT_OUTER_JOIN = " LEFT OUTER JOIN ";
   public static final String ON = " ON ";
   public static final String NOT_EQUAL = "<>";
   public static final String CREATE_TABLE = "CREATE TABLE ";
   public static final String DROP_TABLE = "DROP TABLE ";
   public static final String CREATE_INDEX = "CREATE INDEX ";
   public static final String NULL = "NULL";
   public static final String IS = " IS ";
   public static final String IN = " IN ";
   public static final String EMPTY = "EMPTY";
   public static final String BETWEEN = " BETWEEN ";
   public static final String LIKE = " LIKE ";
   public static final String MEMBER_OF = " MEMBER OF ";
   public static final String ESCAPE = " ESCAPE ";
   public static final String CONCAT = "CONCAT";
   public static final String SUBSTRING = "SUBSTRING";
   public static final String LCASE = "LCASE";
   public static final String UCASE = "UCASE";
   public static final String LENGTH = "LENGTH";
   public static final String LOCATE = "LOCATE";
   public static final String ABS = "ABS";
   public static final String SQRT = "SQRT";
   public static final String COUNT = "COUNT";
   public static final String MAX = "MAX";
   public static final String MIN = "MIN";
   public static final String AVG = "AVG";
   public static final String SUM = "SUM";
   public static final String ASC = " ASC";
   public static final String DESC = " DESC";
   public static final String OFFSET = " OFFSET ";
   public static final String LIMIT = " LIMIT ";
   public static final String UPDATE = "UPDATE ";
   public static final String SET = " SET ";
   private static final String DOT = ".";

   private static final String EQ_QUESTMARK = "=?";

   private static final Vector rwords = new Vector();

   public static String fixTableName(String tableName, DataSource dataSource)
      throws DeploymentException
   {
      // Separate schema name and table name
      String strSchema = "";
      int iIndex;
      if((iIndex = tableName.indexOf('.')) != -1)
      {
         strSchema = tableName.substring(0, iIndex);
         tableName = tableName.substring(iIndex + 1);
      }

      // check for SQL reserved word and escape it with prepending a "X"
      // IMHO one should reject reserved words and throw a
      // DeploymentException - pilhuhn
      if(rwords != null)
      {
         for(int i = 0; i < rwords.size(); i++)
         {
            if(((String)rwords.elementAt(i)).equalsIgnoreCase(tableName))
            {
               tableName = "X" + tableName;
               break;
            }
         }
      }

      Connection con = null;
      try
      {
         con = dataSource.getConnection();
         DatabaseMetaData dmd = con.getMetaData();

         // fix length
         int maxLength = dmd.getMaxTableNameLength();
         if(maxLength > 0 && tableName.length() > maxLength)
         {
            CRC32 crc = new CRC32();
            crc.update(tableName.getBytes());
            String nameCRC = Long.toString(crc.getValue(), 36);

            tableName = tableName.substring(
               0,
               maxLength - nameCRC.length() - 2);
            tableName += "_" + nameCRC;
         }

         // fix case
         if(dmd.storesLowerCaseIdentifiers())
         {
            tableName = tableName.toLowerCase();
         }
         else if(dmd.storesUpperCaseIdentifiers())
         {
            tableName = tableName.toUpperCase();
         }
         // now put the schema name back on the table name
         if(strSchema.length() > 0)
         {
            tableName = strSchema + "." + tableName;
         }
         return tableName;
      }
      catch(SQLException e)
      {
         // This should not happen. A J2EE compatiable JDBC driver is
         // required fully support metadata.
         throw new DeploymentException("Error while fixing table name", e);
      }
      finally
      {
         JDBCUtil.safeClose(con);
      }
   }

   public static void addToRwords(String word)
   {
      if(!rwords.contains(word))
         rwords.add(word);
   }


   public static String fixConstraintName(String name, DataSource dataSource)
      throws DeploymentException
   {
      return fixTableName(name, dataSource).replace('.', '_');
   }

   // =======================================================================
   //  Create Table Columns Clause
   //    columnName0 sqlType0
   //    [, columnName1 sqlType0
   //    [, columnName2 sqlType0 [...]]]
   // =======================================================================
   public static String getCreateTableColumnsClause(JDBCFieldBridge[] fields)
   {
      StringBuffer buf = new StringBuffer(100);
      boolean comma = false;
      for(int i = 0; i < fields.length; ++i)
      {
         JDBCType type = getJDBCType(fields[i]);
         if(type != null)
         {
            if(comma)
               buf.append(COMMA);
            else
               comma = true;
            buf.append(getCreateTableColumnsClause(type));
         }
      }
      return buf.toString();
   }

   /**
    * Returns columnName0 sqlType0
    *    [, columnName1 sqlType0
    *    [, columnName2 sqlType0 [...]]]
    */
   public static String getCreateTableColumnsClause(JDBCType type)
   {
      String[] columnNames = type.getColumnNames();
      String[] sqlTypes = type.getSQLTypes();
      boolean[] notNull = type.getNotNull();

      StringBuffer buf = new StringBuffer();
      for(int i = 0; i < columnNames.length; i++)
      {
         if(i != 0)
            buf.append(COMMA);
         buf.append(columnNames[i]).append(' ').append(sqlTypes[i]);
         if(notNull[i])
            buf.append(NOT).append(NULL);
      }
      return buf.toString();
   }

   // =======================================================================
   //  Column Names Clause
   //    columnName0 [, columnName1 [AND columnName2 [...]]]
   // =======================================================================

   /**
    * Returns columnName0 [, columnName1 [AND columnName2 [...]]]
    */
   public static StringBuffer getColumnNamesClause(JDBCFieldBridge[] fields, StringBuffer sb)
   {
      return getColumnNamesClause(fields, "", sb);
   }

   /**
    * Returns columnName0 [, columnName1 [AND columnName2 [...]]]
    */
   public static StringBuffer getColumnNamesClause(JDBCFieldBridge[] fields,
                                                   String identifier,
                                                   StringBuffer buf)
   {
      boolean comma = false;
      for(int i = 0; i < fields.length; ++i)
      {
         JDBCType type = getJDBCType(fields[i]);
         if(type != null)
         {
            if(comma)
               buf.append(COMMA);
            else
               comma = true;
            getColumnNamesClause(type, identifier, buf);
         }
      }
      return buf;
   }

   /**
    * Returns columnName0 [, columnName1 [AND columnName2 [...]]]
    */
   public static StringBuffer getColumnNamesClause(JDBCEntityBridge entity, String eagerLoadGroup, StringBuffer sb)
   {
      return getColumnNamesClause(entity, eagerLoadGroup, "", sb);
   }

   /**
    * Returns columnName0 [, columnName1 [AND columnName2 [...]]]
    */
   public static StringBuffer getColumnNamesClause(JDBCEntityBridge entity,
                                                   String eagerLoadGroup,
                                                   String alias,
                                                   StringBuffer sb)
   {
      return getColumnNamesClause(entity.getTableFields(), entity.getLoadGroupMask(eagerLoadGroup), alias, sb);
   }

   /**
    * Returns columnName0 [, columnName1 [AND columnName2 [...]]]
    */
   public static StringBuffer getColumnNamesClause(JDBCEntityBridge.FieldIterator loadIter, StringBuffer sb)
   {
      if(loadIter.hasNext())
         getColumnNamesClause(loadIter.next(), sb);
      while(loadIter.hasNext())
      {
         sb.append(COMMA);
         getColumnNamesClause(loadIter.next(), sb);
      }
      return sb;
   }

   /**
    * Returns columnName0 [, columnName1 [AND columnName2 [...]]]
    */
   public static StringBuffer getColumnNamesClause(JDBCFieldBridge[] fields,
                                                   boolean[] mask,
                                                   String identifier,
                                                   StringBuffer buf)
   {
      boolean comma = false;
      for(int i = 0; i < fields.length; ++i)
      {
         if(mask[i])
         {
            JDBCType type = getJDBCType(fields[i]);
            if(type != null)
            {
               if(comma)
                  buf.append(COMMA);
               else
                  comma = true;
               getColumnNamesClause(type, identifier, buf);
            }
         }
      }
      return buf;
   }

   /**
    * Returns columnName0 [, columnName1 [, columnName2 [...]]]
    */
   public static StringBuffer getColumnNamesClause(JDBCFieldBridge field, StringBuffer sb)
   {
      return getColumnNamesClause(field.getJDBCType(), sb);
   }

   /**
    * Returns identifier.columnName0
    *    [, identifier.columnName1
    *    [, identifier.columnName2 [...]]]
    */
   public static StringBuffer getColumnNamesClause(JDBCFieldBridge field, String identifier, StringBuffer sb)
   {
      return getColumnNamesClause(field.getJDBCType(), identifier, sb);
   }

   /**
    * Returns identifier.columnName0
    *    [, identifier.columnName1
    *    [, identifier.columnName2 [...]]]
    */
   private static StringBuffer getColumnNamesClause(JDBCType type, String identifier, StringBuffer buf)
   {
      String[] columnNames = type.getColumnNames();
      boolean hasIdentifier = identifier.length() > 0;
      if(hasIdentifier)
         buf.append(identifier).append(DOT);
      buf.append(columnNames[0]);
      int i = 1;
      while(i < columnNames.length)
      {
         buf.append(COMMA);
         if(hasIdentifier)
            buf.append(identifier).append(DOT);
         buf.append(columnNames[i++]);
      }
      return buf;
   }

   /**
    * Returns identifier.columnName0
    *    [, identifier.columnName1
    *    [, identifier.columnName2 [...]]]
    */
   private static StringBuffer getColumnNamesClause(JDBCType type, StringBuffer buf)
   {
      String[] columnNames = type.getColumnNames();
      buf.append(columnNames[0]);
      int i = 1;
      while(i < columnNames.length)
      {
         buf.append(COMMA).append(columnNames[i++]);
      }
      return buf;
   }

   // =======================================================================
   //  Set Clause
   //    columnName0=? [, columnName1=? [, columnName2=? [...]]]
   // =======================================================================

   /**
    * Returns columnName0=? [, columnName1=? [, columnName2=? [...]]]
    */
   public static StringBuffer getSetClause(JDBCEntityBridge.FieldIterator fieldsIter,
                                           StringBuffer buf)
   {
      JDBCType type = getJDBCType(fieldsIter.next());
      getSetClause(type, buf);
      while(fieldsIter.hasNext())
      {
         type = getJDBCType(fieldsIter.next());
         buf.append(COMMA);
         getSetClause(type, buf);
      }
      return buf;
   }

   /**
    * Returns columnName0=? [, columnName1=? [, columnName2=? [...]]]
    */
   private static StringBuffer getSetClause(JDBCType type, StringBuffer buf)
   {
      String[] columnNames = type.getColumnNames();
      buf.append(columnNames[0]).append(EQ_QUESTMARK);
      int i = 1;
      while(i < columnNames.length)
      {
         buf.append(COMMA).append(columnNames[i++]).append(EQ_QUESTMARK);
      }
      return buf;
   }

   // =======================================================================
   //  Values Clause
   //    ? [, ? [, ? [...]]]
   // =======================================================================

   /**
    * Returns ? [, ? [, ? [...]]]
    */
   public static StringBuffer getValuesClause(JDBCFieldBridge[] fields, StringBuffer buf)
   {
      boolean comma = false;
      for(int i = 0; i < fields.length; ++i)
      {
         JDBCType type = getJDBCType(fields[i]);
         if(type != null)
         {
            if(comma)
               buf.append(COMMA);
            else
               comma = true;
            getValuesClause(type, buf);
         }
      }
      return buf;
   }

   /**
    * Returns ? [, ? [, ? [...]]]
    */
   private static StringBuffer getValuesClause(JDBCType type, StringBuffer buf)
   {
      int columnCount = type.getColumnNames().length;
      buf.append('?');
      int i = 1;
      while(i++ < columnCount)
         buf.append(COMMA).append('?');
      return buf;
   }

   // =======================================================================
   //  Where Clause
   //    columnName0=? [AND columnName1=? [AND columnName2=? [...]]]
   // =======================================================================

   /**
    * Returns columnName0=? [AND columnName1=? [AND columnName2=? [...]]]
    */
   public static StringBuffer getWhereClause(JDBCFieldBridge[] fields, StringBuffer buf)
   {
      return getWhereClause(fields, "", buf);
   }

   /**
    * Returns identifier.columnName0=?
    *    [AND identifier.columnName1=?
    *    [AND identifier.columnName2=? [...]]]
    */
   public static StringBuffer getWhereClause(JDBCFieldBridge[] fields, String identifier, StringBuffer buf)
   {
      boolean and = false;
      for(int i = 0; i < fields.length; ++i)
      {
         JDBCType type = getJDBCType(fields[i]);
         if(type != null)
         {
            if(and)
               buf.append(AND);
            else
               and = true;
            getWhereClause(type, identifier, buf);
         }
      }
      return buf;
   }

   /**
    * Returns columnName0=? [AND columnName1=? [AND columnName2=? [...]]]
    */
   public static StringBuffer getWhereClause(JDBCFieldBridge[] fields,
                                             long mask,
                                             StringBuffer buf)
   {
      return getWhereClause(fields, mask, "", buf);
   }

   /**
    * Returns columnName0=? [AND columnName1=? [AND columnName2=? [...]]]
    */
   private static StringBuffer getWhereClause(JDBCFieldBridge[] fields,
                                              long mask,
                                              String identifier,
                                              StringBuffer buf)
   {
      boolean and = false;
      long fieldMask = 1;
      for(int i = 0; i < fields.length; ++i)
      {
         if((fieldMask & mask) > 0)
         {
            JDBCType type = getJDBCType(fields[i]);
            if(type != null)
            {
               if(and)
                  buf.append(AND);
               else
                  and = true;
               getWhereClause(type, identifier, buf);
            }
         }
         fieldMask <<= 1;
      }
      return buf;
   }

   /**
    * Returns columnName0=? [AND columnName1=? [AND columnName2=? [...]]]
    */
   public static StringBuffer getWhereClause(JDBCFieldBridge field, StringBuffer buf)
   {
      return getWhereClause(field.getJDBCType(), "", buf);
   }

   /**
    * Returns identifier.columnName0=?
    *    [AND identifier.columnName1=?
    *    [AND identifier.columnName2=? [...]]]
    */
   public static StringBuffer getWhereClause(JDBCType type, String identifier, StringBuffer buf)
   {
      if(identifier.length() > 0)
      {
         identifier += '.';
      }

      String[] columnNames = type.getColumnNames();
      buf.append(identifier).append(columnNames[0]).append(EQ_QUESTMARK);
      int i = 1;
      while(i < columnNames.length)
      {
         buf.append(AND).append(identifier).append(columnNames[i++]).append(EQ_QUESTMARK);
      }
      return buf;
   }


   // =======================================================================
   //  Is [Not] Null Clause
   //    columnName0 IS [NOT] NULL [AND columnName1 IS [NOT] NULL [...]]
   // =======================================================================

   /**
    * Returns identifier.columnName0 IS [NOT] NULL
    *    [AND identifier.columnName1 IS [NOT] NULL
    *    [AND identifier.columnName2 IS [NOT] NULL [...]]]
    */
   public static StringBuffer getIsNullClause(boolean not,
                                              JDBCFieldBridge[] fields,
                                              String identifier,
                                              StringBuffer buf)
   {
      boolean and = false;
      for(int i = 0; i < fields.length; ++i)
      {
         JDBCType type = getJDBCType(fields[i]);
         if(type != null)
         {
            if(and)
               buf.append(AND);
            else
               and = true;
            getIsNullClause(not, type, identifier, buf);
         }
      }
      return buf;
   }

   /**
    * Returns identifier.columnName0 IS [NOT] NULL
    *    [AND identifier.columnName1 IS [NOT] NULL
    *    [AND identifier.columnName2 IS [NOT] NULL [...]]]
    */
   public static StringBuffer getIsNullClause(boolean not,
                                              JDBCFieldBridge field,
                                              String identifier,
                                              StringBuffer buf)
   {
      return getIsNullClause(not, field.getJDBCType(), identifier, buf);
   }

   /**
    * Returns identifier.columnName0 IS [NOT] NULL
    *    [AND identifier.columnName1 IS [NOT] NULL
    *    [AND identifier.columnName2 IS [NOT] NULL [...]]]
    */
   private static StringBuffer getIsNullClause(boolean not,
                                               JDBCType type,
                                               String identifier,
                                               StringBuffer buf)
   {
      if(identifier.length() > 0)
      {
         identifier += '.';
      }

      String[] columnNames = type.getColumnNames();

      buf.append(identifier).append(columnNames[0]).append(IS);
      (not ? buf.append(NOT) : buf).append(NULL);
      int i = 1;
      while(i < columnNames.length)
      {
         buf.append(AND).append(identifier).append(columnNames[i++]).append(IS);
         (not ? buf.append(NOT) : buf).append(NULL);
      }
      return buf;
   }

   // =======================================================================
   //  Join Clause
   //    parent.pkColumnName0=child.fkColumnName0
   //    [AND parent.pkColumnName1=child.fkColumnName1
   //    [AND parent.pkColumnName2=child.fkColumnName2 [...]]]
   // =======================================================================

   public static StringBuffer getJoinClause(JDBCCMRFieldBridge cmrField,
                                            String parentAlias,
                                            String childAlias,
                                            StringBuffer buf)
   {
      JDBCEntityBridge parentEntity = cmrField.getEntity();
      JDBCEntityBridge childEntity = (JDBCEntityBridge)cmrField.getRelatedEntity();

      JDBCCMPFieldBridge parentField;
      JDBCCMPFieldBridge childField;

      if(cmrField.hasForeignKey())
      {
         // parent has the foreign keys
         JDBCCMPFieldBridge[] parentFkFields = cmrField.getForeignKeyFields();
         int i = 0;
         while(i < parentFkFields.length)
         {
            parentField = parentFkFields[i++];
            childField = childEntity.getCMPFieldByName(parentField.getFieldName());
            getJoinClause(parentField, parentAlias, childField, childAlias, buf);
            if(i < parentFkFields.length)
               buf.append(AND);
         }
      }
      else
      {
         // child has the foreign keys
         JDBCCMPFieldBridge[] childFkFields = cmrField.getRelatedCMRField().getForeignKeyFields();
         int i = 0;
         while(i < childFkFields.length)
         {
            childField = childFkFields[i++];
            parentField = parentEntity.getCMPFieldByName(childField.getFieldName());

            // add the sql
            getJoinClause(parentField, parentAlias, childField, childAlias, buf);
            if(i < childFkFields.length)
            {
               buf.append(AND);
            }
         }
      }
      return buf;
   }

   public static StringBuffer getRelationTableJoinClause(JDBCCMRFieldBridge cmrField,
                                                         String parentAlias,
                                                         String relationTableAlias,
                                                         StringBuffer buf)
   {
      JDBCEntityBridge parentEntity = cmrField.getEntity();
      JDBCCMPFieldBridge parentField;
      JDBCCMPFieldBridge relationField;

      // parent to relation table join
      JDBCCMPFieldBridge[] parentFields = cmrField.getTableKeyFields();
      int i = 0;
      while(i < parentFields.length)
      {
         relationField = parentFields[i++];
         parentField = parentEntity.getCMPFieldByName(relationField.getFieldName());
         getJoinClause(parentField, parentAlias, relationField, relationTableAlias, buf);
         if(i < parentFields.length)
            buf.append(AND);
      }
      return buf;
   }

   /**
    * Returns parent.pkColumnName0=child.fkColumnName0
    *    [AND parent.pkColumnName1=child.fkColumnName1
    *    [AND parent.pkColumnName2=child.fkColumnName2 [...]]]
    */
   private static StringBuffer getJoinClause(JDBCFieldBridge pkField,
                                             String parent,
                                             JDBCFieldBridge fkField,
                                             String child,
                                             StringBuffer buf)
   {
      return getJoinClause(pkField.getJDBCType(), parent, fkField.getJDBCType(), child, buf);
   }

   public static StringBuffer getJoinClause(JDBCFieldBridge[] pkFields,
                                            String parent,
                                            JDBCFieldBridge[] fkFields,
                                            String child,
                                            StringBuffer buf)
   {
      if(pkFields.length != fkFields.length)
      {
         throw new IllegalArgumentException(
            "Error createing theta join clause:" +
            " pkField.size()=" + pkFields.length +
            " fkField.size()=" + fkFields.length);
      }

      boolean and = false;
      for(int i = 0; i < pkFields.length; ++i)
      {
         // these types should not be null
         JDBCType pkType = getJDBCType(pkFields[i]);
         JDBCType fkType = getJDBCType(fkFields[i]);
         if(and)
            buf.append(AND);
         else
            and = true;
         getJoinClause(pkType, parent, fkType, child, buf);
      }
      return buf;
   }

   /**
    * Returns parent.pkColumnName0=child.fkColumnName0
    *    [AND parent.pkColumnName1=child.fkColumnName1
    *    [AND parent.pkColumnName2=child.fkColumnName2 [...]]]
    */
   private static StringBuffer getJoinClause(JDBCType pkType,
                                             String parent,
                                             JDBCType fkType,
                                             String child,
                                             StringBuffer buf)
   {
      if(parent.length() > 0)
      {
         parent += '.';
      }
      if(child.length() > 0)
      {
         child += '.';
      }

      String[] pkColumnNames = pkType.getColumnNames();
      String[] fkColumnNames = fkType.getColumnNames();
      if(pkColumnNames.length != fkColumnNames.length)
      {
         throw new IllegalArgumentException("PK and FK have different number of columns");
      }

      buf.append(parent).append(pkColumnNames[0]).append('=').append(child).append(fkColumnNames[0]);
      int i = 1;
      while(i < pkColumnNames.length)
      {
         buf.append(AND)
            .append(parent)
            .append(pkColumnNames[i])
            .append('=')
            .append(child)
            .append(fkColumnNames[i++]);
      }
      return buf;
   }

   // =======================================================================
   //  Self Compare Where Clause
   //    fromIdentifier.pkColumnName0=toIdentifier.fkColumnName0
   //    [AND fromIdentifier.pkColumnName1=toIdentifier.fkColumnName1
   //    [AND fromIdentifier.pkColumnName2=toIdentifier.fkColumnName2 [...]]]
   // =======================================================================

   public static StringBuffer getSelfCompareWhereClause(JDBCFieldBridge[] fields,
                                                        String fromIdentifier,
                                                        String toIdentifier,
                                                        StringBuffer buf)
   {
      boolean and = false;
      for(int i = 0; i < fields.length; ++i)
      {
         JDBCType type = getJDBCType(fields[i]);
         if(type != null)
         {
            if(and)
               buf.append(AND);
            else
               and = true;
            getSelfCompareWhereClause(type, fromIdentifier, toIdentifier, buf);
         }
      }
      return buf;
   }

   private static StringBuffer getSelfCompareWhereClause(JDBCType type,
                                                         String fromIdentifier,
                                                         String toIdentifier,
                                                         StringBuffer buf)
   {
      if(fromIdentifier.length() > 0)
         fromIdentifier += '.';
      if(toIdentifier.length() > 0)
         toIdentifier += '.';

      String[] columnNames = type.getColumnNames();

      buf.append(fromIdentifier)
         .append(columnNames[0])
         .append('=')
         .append(toIdentifier)
         .append(columnNames[0]);
      int i = 1;
      while(i < columnNames.length)
      {
         buf.append(AND)
            .append(fromIdentifier)
            .append(columnNames[i])
            .append('=')
            .append(toIdentifier)
            .append(columnNames[i++]);
      }
      return buf;
   }

   public static StringBuffer getSelfCompareWhereClause(JDBCFieldBridge fromField,
                                                        JDBCFieldBridge toField,
                                                        String fromIdentifier,
                                                        String toIdentifier,
                                                        StringBuffer buf)
   {
      return getSelfCompareWhereClause(
         fromField.getJDBCType(), toField.getJDBCType(), fromIdentifier, toIdentifier, buf
      );
   }

   private static StringBuffer getSelfCompareWhereClause(JDBCType fromType,
                                                         JDBCType toType,
                                                         String fromIdentifier,
                                                         String toIdentifier,
                                                         StringBuffer buf)
   {
      if(fromIdentifier.length() > 0)
         fromIdentifier += '.';
      if(toIdentifier.length() > 0)
         toIdentifier += '.';

      String[] fromColumnNames = fromType.getColumnNames();
      String[] toColumnNames = toType.getColumnNames();

      buf.append(fromIdentifier)
         .append(fromColumnNames[0])
         .append('=')
         .append(toIdentifier)
         .append(toColumnNames[0]);
      int i = 1;
      while(i < fromColumnNames.length)
      {
         buf.append(AND)
            .append(fromIdentifier)
            .append(fromColumnNames[i])
            .append('=')
            .append(toIdentifier)
            .append(toColumnNames[i++]);
      }
      return buf;
   }

   public static boolean tableExists(String tableName, DataSource dataSource)
      throws DeploymentException
   {
      Connection con = null;
      ResultSet rs = null;
      try
      {
         con = dataSource.getConnection();

         // (a j2ee spec compatible jdbc driver has to fully
         // implement the DatabaseMetaData)
         DatabaseMetaData dmd = con.getMetaData();
         String catalog = con.getCatalog();
         String schema = null;
         String quote = dmd.getIdentifierQuoteString();
         if(tableName.startsWith(quote))
         {
            if(tableName.endsWith(quote) == false)
            {
               throw new DeploymentException("Mismatched quote in table name: " + tableName);
            }
            int quoteLength = quote.length();
            tableName = tableName.substring(quoteLength, tableName.length() - quoteLength);
            if(dmd.storesLowerCaseQuotedIdentifiers())
               tableName = tableName.toLowerCase();
            else if(dmd.storesUpperCaseQuotedIdentifiers())
               tableName = tableName.toUpperCase();
         }
         else
         {
            if(dmd.storesLowerCaseIdentifiers())
               tableName = tableName.toLowerCase();
            else if(dmd.storesUpperCaseIdentifiers())
               tableName = tableName.toUpperCase();
         }
         rs = dmd.getTables(catalog, schema, tableName, null);
         return rs.next();
      }
      catch(SQLException e)
      {
         // This should not happen. A J2EE compatiable JDBC driver is
         // required fully support metadata.
         throw new DeploymentException("Error while checking if table aleady exists", e);
      }
      finally
      {
         JDBCUtil.safeClose(rs);
         JDBCUtil.safeClose(con);
      }
   }

   private static JDBCType getJDBCType(JDBCFieldBridge field)
   {
      JDBCType type = field.getJDBCType();
      if(type != null && type.getColumnNames().length > 0)
      {
         return type;
      }
      return null;
   }
}
