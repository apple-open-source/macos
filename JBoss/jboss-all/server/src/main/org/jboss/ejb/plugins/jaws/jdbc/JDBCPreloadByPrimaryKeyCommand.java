/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.jaws.jdbc;

import java.sql.PreparedStatement;


/**
 * JDBCPreloadByPrimaryKey
 *
 * This finder be called on when read-ahead is turned on and findByPrimaryKey
 * is called.  It will read-ahead instead of just the old exists logic.
 *
 * @see <related>
 * @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>
 * @version $Revision: 1.3 $
 */
public class JDBCPreloadByPrimaryKeyCommand extends JDBCPreloadFinderCommand
{
   // Constructors --------------------------------------------------

   public JDBCPreloadByPrimaryKeyCommand(JDBCCommandFactory factory)
   {
      super(factory, "PreloadByPrimaryKey");
      String sql = loadCommand.createSelectClause() + " FROM " + jawsEntity.getTableName() +
                   " WHERE " + getPkColumnWhereList();
      setSQL(sql);
   }

   // Public --------------------------------------------------------

   // JDBCQueryCommand overrides ------------------------------------

   protected void setParameters(PreparedStatement stmt, Object argOrArgs)
      throws Exception
   {
       Object[] objects = (Object[])argOrArgs;
       setPrimaryKeyParameters(stmt, 1, objects[0]);
   }
}
