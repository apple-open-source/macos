/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.jaws.jdbc;


import org.jboss.ejb.plugins.jaws.JPMDestroyCommand;

import org.jboss.logging.Logger;

/**
 * JAWSPersistenceManager JDBCDestroyCommand
 *
 * @see <related>
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @author <a href="mailto:shevlandj@kpi.com.au">Joe Shevland</a>
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
public class JDBCDestroyCommand
   extends JDBCUpdateCommand
   implements JPMDestroyCommand
{
   // Attributes ----------------------------------------------------

   private Logger log = Logger.getLogger(JDBCDestroyCommand.class);

   // Constructors --------------------------------------------------
   
   public JDBCDestroyCommand(JDBCCommandFactory factory)
   {
      super(factory, "Destroy");
      
      // Drop table SQL
      String sql = "DROP TABLE " + jawsEntity.getTableName();
      setSQL(sql);
   }
   
   // JPMDestroyCommand implementation ------------------------------
   
   public void execute()
   {
      if (jawsEntity.getRemoveTable())
      {
         // Remove it!
         try
         {
            // since we use the pools, we have to do this within a transaction
            factory.getContainer().getTransactionManager().begin ();
            jdbcExecute(null);
            factory.getContainer().getTransactionManager().commit ();
         } catch (Exception e)
         {
            if (log.isDebugEnabled())
               log.debug("Could not drop table " +
                      jawsEntity.getTableName(), e);

            try
            {
               factory.getContainer().getTransactionManager().rollback ();
            }
            catch (Exception _e)
            {
               log.error("Could not roll back transaction", _e);
            }
         }
      }
   }
   
   // JDBCUpdateCommand overrides -----------------------------------
   
   protected Object handleResult(int rowsAffected, Object argOrArgs) 
      throws Exception
   {
      if (log.isDebugEnabled())
         log.debug("Table "+jawsEntity.getTableName()+" removed");
      
      return null;
   }
}
