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
 * Abstract superclass for all JAWS Commands that issue JDBC queries
 * directly.
 * Provides a Template Method implementation for
 * <code>executeStatementAndHandleResult</code>.
 * 
 * @author <a href="mailto:justin@j-m-f.demon.co.uk">Justin Forder</a>
 * @version $Revision: 1.8 $
 */
public abstract class JDBCQueryCommand
   extends JDBCCommand
{
   // Constructors --------------------------------------------------

   private Logger log = Logger.getLogger(JDBCFindByCommand.class);
   
   /**
    * Pass the arguments on to the superclass constructor.
    */
   protected JDBCQueryCommand(JDBCCommandFactory factory, String name)
   {
      super(factory, name);
   }
   
   // Protected -----------------------------------------------------
   
   /**
    * Template Method that executes the PreparedStatement and calls
    * <code>handleResult</code> on the resulting ResultSet.
    *
    * @param stmt the prepared statement, with its parameters already set.
    * @param argOrArgs argument or array of arguments passed in from
    *  subclass execute method.
    * @return any result produced by the handling of the result of executing
    *  the prepared statement.
    * @throws Exception if execution or result handling fails.
    */
   protected Object executeStatementAndHandleResult(PreparedStatement stmt,
                                                    Object argOrArgs)
      throws Exception
   {
      ResultSet rs = null;
      Object result = null;
      
      try
      {
         rs = stmt.executeQuery();
         result = handleResult(rs, argOrArgs);
      } finally
      {
         if (rs != null)
         {
            try
            {
               rs.close();
            } catch (SQLException e)
            {
               log.debug("failed to close resultset", e);
            }
         }
      }
      
      return result;
   }
   
   /**
    * Handles the result of successful execution of the query.
    *
    * @param rs the result set from the query.
    * @param argOrArgs argument or array of arguments passed in from
    *  subclass execute method.
    * @return any result produced by the handling of the result of executing
    *  the prepared statement.
    * @throws Exception if execution or result handling fails.
    */
   protected abstract Object handleResult(ResultSet rs, Object argOrArgs) 
      throws Exception;
}
