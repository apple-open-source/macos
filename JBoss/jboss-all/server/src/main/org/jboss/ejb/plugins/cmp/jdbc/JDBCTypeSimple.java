/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc;


/**
 * This class provides a simple mapping of a Java type type to a single column.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
 * @version $Revision: 1.4.4.6 $
 */
public final class JDBCTypeSimple implements JDBCType
{
   private final String[] columnNames;
   private final Class[] javaTypes;
   private final int[] jdbcTypes;
   private final String[] sqlTypes;
   private final boolean[] notNull;
   private final boolean[] autoIncrement;
   private final JDBCUtil.ResultSetReader[] resultSetReader;

   private final Mapper mapper;

   public JDBCTypeSimple(
      String columnName,
      Class javaType,
      int jdbcType,
      String sqlType,
      boolean notNull,
      boolean autoIncrement,
      Mapper mapper
      )
   {
      columnNames = new String[]{columnName};
      javaTypes = new Class[]{javaType};
      jdbcTypes = new int[]{jdbcType};
      sqlTypes = new String[]{sqlType};
      this.notNull = new boolean[]{notNull};
      this.autoIncrement = new boolean[]{autoIncrement};
      this.mapper = mapper;
      resultSetReader = new JDBCUtil.ResultSetReader[]{JDBCUtil.getResultSetReader(jdbcType, javaType)};
   }

   public final String[] getColumnNames()
   {
      return columnNames;
   }

   public final Class[] getJavaTypes()
   {
      return javaTypes;
   }

   public final int[] getJDBCTypes()
   {
      return jdbcTypes;
   }

   public final String[] getSQLTypes()
   {
      return sqlTypes;
   }

   public final boolean[] getNotNull()
   {
      return notNull;
   }

   public final boolean[] getAutoIncrement()
   {
      return autoIncrement;
   }

   public final Object getColumnValue(int index, Object value)
   {
      if(index != 0)
      {
         throw new IndexOutOfBoundsException("JDBCSimpleType does not support an index>0.");
      }
      return mapper == null ? value : mapper.toColumnValue(value);
   }

   public final Object setColumnValue(int index, Object value, Object columnValue)
   {
      if(index != 0)
      {
         throw new IndexOutOfBoundsException("JDBCSimpleType does not support an index>0.");
      }
      return mapper == null ? columnValue : mapper.toFieldValue(columnValue);
   }

   public final JDBCUtil.ResultSetReader[] getResultSetReaders()
   {
      return resultSetReader;
   }
}
