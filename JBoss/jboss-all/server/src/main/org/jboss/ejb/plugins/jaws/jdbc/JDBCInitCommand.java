/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.jaws.jdbc;

import java.util.Iterator;

import org.jboss.ejb.plugins.jaws.JPMInitCommand;
import org.jboss.ejb.plugins.jaws.metadata.CMPFieldMetaData;
import org.jboss.ejb.plugins.jaws.metadata.PkFieldMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.SQLUtil;

import org.jboss.logging.Logger;

/**
 * JAWSPersistenceManager JDBCInitCommand
 *
 * @see <related>
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @author <a href="mailto:shevlandj@kpi.com.au">Joe Shevland</a>
 * @author <a href="mailto:justin@j-m-f.demon.co.uk">Justin Forder</a>
 * @author <a href="mailto:michel.anke@wolmail.nl">Michel de Groot</a>
 * @author <a href="mailto:david_jencks@earthlink.net">David Jencks</a>
 * @author <a href="mailto:danch@nvisia.com">danch (Dan Christopherson</a>
 *
 * @version $Revision: 1.19.4.1 $
 *
 *   <p><b>Revisions:</b>
 *
 *   <p><b>20010621 danch</b>:
 *   <ul>
 *   <li>merged patch from David Jenks - null constraint on columns.
 *   <li>fixed bug where remapping column name of key field caused an invalid
 *   PK constraint to be build.
 *   </ul>
 *
 *   <p><b>20010812 vincent.harcq@hubmethods.com:</b>
 *   <ul>
 *   <li> Get Rid of debug flag, use log4j instead
 *   <li> Correct pk-constraint to not only work when PK class is primitive
 *   </ul>
 *
 */
public class JDBCInitCommand
   extends JDBCUpdateCommand
   implements JPMInitCommand
{
   // Attributes ----------------------------------------------------
   private Logger log = Logger.getLogger(JDBCInitCommand.class);

   // Constructors --------------------------------------------------

   public JDBCInitCommand(JDBCCommandFactory factory)
   {
      super(factory, "Init");

      // Create table SQL
      String sql = "CREATE TABLE " + jawsEntity.getTableName() + " (";

      Iterator it = jawsEntity.getCMPFields();
      boolean first = true;
      while (it.hasNext())
      {
         CMPFieldMetaData cmpField = (CMPFieldMetaData)it.next();

         sql += (first ? "" : ",") +
                cmpField.getColumnName() + " " +
                cmpField.getSQLType() +
                cmpField.getNullable();


         first = false;
      }

      // If there is a primary key field,
      // and the bean has explicitly <pk-constraint>true</pk-constraint> in jaws.xml
      // add primary key constraint.
      if (jawsEntity.hasPkConstraint())
      {
         sql += ",CONSTRAINT pk"+jawsEntity.getTableName()+" PRIMARY KEY (";
         for (Iterator i = jawsEntity.getPkFields();i.hasNext();) {
            String keyCol = ((PkFieldMetaData)i.next()).getColumnName();
            sql += keyCol;
            sql += i.hasNext()?",":"";
         }
         sql +=")";
      }

      sql += ")";

      setSQL(sql);
   }

   // JPMInitCommand implementation ---------------------------------

   public void execute() throws Exception
   {
      // Create table if necessary
      if (jawsEntity.getCreateTable())
      {
         // first check if the table already exists...
         boolean created = SQLUtil.tableExists(jawsEntity.getTableName(), jawsEntity.getDataSource());

         // Try to create it
         boolean infoEnabled = log.isInfoEnabled();
         boolean debug = log.isDebugEnabled();
         if(created) {
             if (infoEnabled)
                 log.info("Table '"+jawsEntity.getTableName()+"' already exists");
         } else {
             try
             {
                // since we use the pools, we have to do this within a transaction
                factory.getContainer().getTransactionManager().begin ();
                jdbcExecute(null);
                factory.getContainer().getTransactionManager().commit ();

                // Create successful, log this
                if (infoEnabled)
                    log.info("Created table '"+jawsEntity.getTableName()+"' successfully.");
	             if (jawsEntity.getPrimKeyField() != null)
	               if (debug)
                    log.debug("Primary key of table '"+jawsEntity.getTableName()+"' is '"
	                     +jawsEntity.getPrimKeyField()+"'.");
	             else {
	             	String flds = "[";
	                for (Iterator i = jawsEntity.getPkFields();i.hasNext();) {
					   flds += ((PkFieldMetaData)i.next()).getName();
					   flds += i.hasNext()?",":"";
					}
				    flds += "]";
            if (debug)
               log.debug("Primary key of table '"+jawsEntity.getTableName()+"' is " + flds);
	             }
	         } catch (Exception e)
             {
                if (debug)
                   log.debug("Could not create table " +
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
   }

   // JDBCUpdateCommand overrides -----------------------------------

   protected Object handleResult(int rowsAffected, Object argOrArgs)
      throws Exception
   {
      if (log.isDebugEnabled())
         log.debug("Table " + jawsEntity.getTableName() + " created");
      return null;
   }
}
