/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.dbschema.util;

import java.util.Map;


/**
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 */
public class Table
{
   private final String table;
   private final Map columnsByName;

   public Table(String table, Map columnsByName)
   {
      this.table = table;
      this.columnsByName = columnsByName;
   }

   public Column getColumn(String name) throws Exception
   {
      Column column = (Column)columnsByName.get(name);
      if(column == null)
         throw new Exception("Column " + name + " not found in table " + table);
      return column;
   }

   public int getColumnsNumber()
   {
      return columnsByName.size();
   }
}
