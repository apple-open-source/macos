/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.jaws.jdbc;

import org.jboss.ejb.plugins.jaws.metadata.FinderMetaData;

/**
 * JAWSPersistenceManager JDBCFindAllCommand
 *
 * @see <related>
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @author <a href="mailto:shevlandj@kpi.com.au">Joe Shevland</a>
 * @author <a href="mailto:justin@j-m-f.demon.co.uk">Justin Forder</a>
 * @version $Revision: 1.6 $
 */
public class JDBCFindAllCommand extends JDBCFinderCommand
{
   // Constructors --------------------------------------------------
   
   public JDBCFindAllCommand(JDBCCommandFactory factory, FinderMetaData f)
   {
      super(factory, f);
      
      String sql = "SELECT " + getPkColumnList() + " FROM " + jawsEntity.getTableName();

      setSQL(sql);
   }

   public String getWhereClause() {
      return "";
   }

   public String getFromClause() {
      return " FROM " + jawsEntity.getTableName();
   }
   public String getOrderByClause() {
      return "";
   }
}
