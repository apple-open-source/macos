/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.dbschema.util;

import java.sql.ResultSet;
import java.sql.SQLException;


/**
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 */
public final class Column
{
   private static final String TABLE_NAME = "TABLE_NAME";
   private static final String COLUMN_NAME = "COLUMN_NAME";
   private static final String DATA_TYPE = "DATA_TYPE";
   private static final String TYPE_NAME = "TYPE_NAME";
   private static final String COLUMN_SIZE = "COLUMN_SIZE";
   private static final String IS_NULLABLE = "IS_NULLABLE";
   private static final String COLUMN_DEF = "COLUMN_DEF";

   private final String tableName;
   private final String name;
   private final short dataType;
   private final String typeName;
   private final int columnSize;
   private final String nullable;
   private final String columnDef;

   // Constructors

   public Column(ResultSet rs) throws SQLException
   {
      tableName = rs.getString(TABLE_NAME);
      name = rs.getString(COLUMN_NAME);
      dataType = rs.getShort(DATA_TYPE);
      typeName = rs.getString(TYPE_NAME);
      columnSize = rs.getInt(COLUMN_SIZE);
      nullable = rs.getString(IS_NULLABLE);
      columnDef = rs.getString(COLUMN_DEF);
   }

   // Public

   public String getTableName()
   {
      return tableName;
   }

   public String getName()
   {
      return name;
   }

   public short getDataType()
   {
      return dataType;
   }

   public String getTypeName()
   {
      return typeName;
   }

   public int getColumnSize()
   {
      return columnSize;
   }

   public boolean isNotNullable()
   {
      return nullable.equalsIgnoreCase("NO");
   }

   public String getColumnDef()
   {
      return columnDef;
   }

   public void assertName(String name) throws Exception
   {
      if(this.name.equals(name))
         return;
      throw new Exception("Column name: is " + this.name + " but expected " + name);
   }

   public void assertDataType(int dataType) throws Exception
   {
      if(this.dataType == dataType)
         return;
      throw new Exception("Data type: is " + this.dataType + " but expected " + dataType);
   }

   public void assertNotNull(boolean notNullable) throws Exception
   {
      if(this.nullable.equalsIgnoreCase("NO") == notNullable)
         return;
      throw new Exception("Column not nullable: is " + !notNullable + " but expected " + notNullable);
   }

   public void assertTypeNotNull(int dataType, boolean notNull) throws Exception
   {
      assertDataType(dataType);
      assertNotNull(notNull);
   }

   public String toString()
   {
      StringBuffer sb = new StringBuffer();
      sb.append('[').
         append(TABLE_NAME).append('=').append(tableName).append(';').
         append(COLUMN_NAME).append('=').append(name).append(';').
         append(DATA_TYPE).append('=').append(dataType).append(';').
         append(TYPE_NAME).append('=').append(typeName).append(';').
         append(COLUMN_SIZE).append('=').append(columnSize).append(';').
         append(IS_NULLABLE).append('=').append(nullable).append(';').
         append(COLUMN_DEF).append('=').append(columnDef).append(';').
         append(']');
      return sb.toString();
   }
}
