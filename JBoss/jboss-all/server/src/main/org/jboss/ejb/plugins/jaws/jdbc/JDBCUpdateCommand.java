/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.jaws.jdbc;

import java.sql.PreparedStatement;

import org.jboss.logging.Logger;

/**
 * Abstract superclass for all JAWS Commands that issue JDBC updates
 * directly.
 * Provides a Template Method implementation for
 * <code>executeStatementAndHandleResult</code>.
 * @author <a href="mailto:justin@j-m-f.demon.co.uk">Justin Forder</a>
 * @version $Revision: 1.8 $
 *
 *   <p><b>Revisions:</b>
 *
 *   <p><b>20010812 vincent.harcq@hubmethods.com:</b>
 *   <ul>
 *   <li> Get Rid of debug flag, use log4j instead
 *   </ul>
 *
 */
public abstract class JDBCUpdateCommand extends JDBCCommand
{
   // Attributes ----------------------------------------------------
   private Logger log = Logger.getLogger(JDBCUpdateCommand.class);

  // Constructors --------------------------------------------------
   
   /**
    * Pass the arguments on to the superclass constructor.
    */
   protected JDBCUpdateCommand(JDBCCommandFactory factory, String name)
   {
      super(factory, name);
   }
   
   // Protected -----------------------------------------------------
   
   /**
    * Template Method that executes the PreparedStatement and calls
    * <code>handleResult</code> on the integer result.
    *
    * @param stmt the prepared statement, with its parameters already set.
    * @param argOrArgs argument or array of arguments passed in from
    *  subclass execute method.
    * @return the result from <code>handleResult</code>.
    * @throws Exception if execution or result handling fails.
    */
   protected Object executeStatementAndHandleResult(PreparedStatement stmt,
                                                    Object argOrArgs)
      throws Exception
   {
      int rowsAffected = stmt.executeUpdate();
      
      if (log.isDebugEnabled())
      {
         log.debug("Rows affected = " + rowsAffected);
      }
      
      return handleResult(rowsAffected, argOrArgs);
   }
   
   /**
    * Handle the result of successful execution of the update.
    
    * @param rs the result set from the query.
    * @param argOrArgs argument or array of arguments passed in from
    *  subclass execute method.
    * @return any result needed by the subclass <code>execute</code>.
    * @throws Exception if result handling fails.
    */
   protected abstract Object handleResult(int rowsAffected, Object argOrArgs) 
      throws Exception;
}
