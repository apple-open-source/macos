/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.jaws.jdbc;

import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;

import org.jboss.logging.Logger;

/**
 * JDBCBeanExistsCommand
 *
 * @see <related>
 * @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @author <a href="mailto:justin@j-m-f.demon.co.uk">Justin Forder</a>
 * @version $Revision: 1.11 $
 *
 *   <p><b>Revisions:</b>
 *
 *   <p><b>20010812 vincent.harcq@hubmethods.com:</b>
 *   <ul>
 *   <li> Get Rid of debug flag, use log4j instead
 *   </ul>
 *
 */
public class JDBCBeanExistsCommand extends JDBCQueryCommand
{
   // Attributes ----------------------------------------------------

   private final Logger log = Logger.getLogger(JDBCBeanExistsCommand.class);

   // Constructors --------------------------------------------------

   public JDBCBeanExistsCommand(JDBCCommandFactory factory)
   {
      super(factory, "Exists");
      String sql = "SELECT COUNT(*) FROM " + jawsEntity.getTableName() +
                   " WHERE " + getPkColumnWhereList();
      setSQL(sql);
   }

   // Public --------------------------------------------------------

   // Checks whether the database already holds the entity

   public boolean execute(Object id)
   {
      boolean result = false;

      try
      {
         result = ((Boolean)jdbcExecute(id)).booleanValue();
      } catch (Exception e)
      {
	      if (log.isDebugEnabled()) log.debug("Exception",e);
      }

      return result;
   }

   // JDBCQueryCommand overrides ------------------------------------

   protected void setParameters(PreparedStatement stmt, Object argOrArgs)
      throws Exception
   {
      setPrimaryKeyParameters(stmt, 1, argOrArgs);
   }

   protected Object handleResult(ResultSet rs, Object argOrArgs) throws Exception
   {
      if ( !rs.next() )
      {
         throw new SQLException("Unable to check for EJB in database");
      }
      int total = rs.getInt(1);
      return new Boolean(total >= 1);
   }
}
