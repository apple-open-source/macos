/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc;

import java.sql.Connection;
import java.sql.PreparedStatement;
import java.util.Iterator;
import javax.ejb.EJBException;
import javax.sql.DataSource;

import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMPFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMRFieldBridge;
import org.jboss.logging.Logger;

/**
 * Deletes relations from a relation table.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 * @version $Revision: 1.10.2.5 $
 */
public final class JDBCDeleteRelationsCommand
{
   private final Logger log;

   public JDBCDeleteRelationsCommand(JDBCStoreManager manager)
   {
      // Create the Log
      log = Logger.getLogger(
         this.getClass().getName() +
         "." +
         manager.getMetaData().getName());
   }

   //
   // This command needs to be changed to chunk delete commands, because
   // some database have a limit on the number of parameters in a statement.
   //
   public void execute(RelationData relationData)
   {
      if(relationData.removedRelations.size() == 0)
      {
         return;
      }

      String sql = createSQL(relationData);

      Connection con = null;
      PreparedStatement ps = null;
      JDBCCMRFieldBridge cmrField = relationData.getLeftCMRField();
      try
      {
         // create the statement
         if(log.isDebugEnabled())
            log.debug("Executing SQL: " + sql);

         // get the connection
         DataSource dataSource = cmrField.getDataSource();
         con = dataSource.getConnection();
         ps = con.prepareStatement(sql);

         // set the parameters
         setParameters(ps, relationData);

         // execute statement
         int rowsAffected = ps.executeUpdate();
         if(log.isDebugEnabled())
            log.debug("Rows affected = " + rowsAffected);
      }
      catch(Exception e)
      {
         throw new EJBException("Could not delete relations from " +
            cmrField.getTableName(), e);
      }
      finally
      {
         JDBCUtil.safeClose(ps);
         JDBCUtil.safeClose(con);
      }
   }

   private static String createSQL(RelationData relationData)
   {
      JDBCCMRFieldBridge left = relationData.getLeftCMRField();
      JDBCCMRFieldBridge right = relationData.getRightCMRField();

      StringBuffer sql = new StringBuffer(300);
      sql.append(SQLUtil.DELETE_FROM)
         .append(left.getTableName())
         .append(SQLUtil.WHERE);

      int removedRelations = relationData.removedRelations.size();
      if(removedRelations > 0)
      {
         StringBuffer whereClause = new StringBuffer(20);
         whereClause.append('(');
            // left keys
            SQLUtil.getWhereClause(left.getTableKeyFields(), whereClause)
               .append(SQLUtil.AND);
            // right keys
            SQLUtil.getWhereClause(right.getTableKeyFields(), whereClause)
               .append(')');
         String whereClauseStr = whereClause.toString();
         sql.append(whereClauseStr);
         for(int i = 1; i < removedRelations; ++i)
         {
            sql.append(SQLUtil.OR).append(whereClauseStr);
         }
      }
      return sql.toString();
   }

   private static void setParameters(PreparedStatement ps, RelationData relationData)
      throws Exception
   {
      int index = 1;
      Iterator pairs = relationData.removedRelations.iterator();
      JDBCCMPFieldBridge[] leftFields = relationData.getLeftCMRField().getTableKeyFields();
      JDBCCMPFieldBridge[] rightFields = relationData.getRightCMRField().getTableKeyFields();
      while(pairs.hasNext())
      {
         RelationPair pair = (RelationPair)pairs.next();

         // left keys
         Object leftId = pair.getLeftId();
         for(int i = 0; i < leftFields.length; ++i)
            index = leftFields[i].setPrimaryKeyParameters(ps, index, leftId);

         // right keys
         Object rightId = pair.getRightId();
         for(int i = 0; i < rightFields.length; ++i)
            index = rightFields[i].setPrimaryKeyParameters(ps, index, rightId);
      }
   }
}
