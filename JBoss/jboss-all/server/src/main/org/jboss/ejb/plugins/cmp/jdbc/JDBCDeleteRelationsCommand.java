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
import java.util.List;
import javax.ejb.EJBException;
import javax.sql.DataSource;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCEntityBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMPFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMRFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCRelationMetaData;
import org.jboss.logging.Logger;

/**
 * Deletes relations from a relation table.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.10 $
 */
public class JDBCDeleteRelationsCommand {
   private JDBCStoreManager manager;
   private JDBCEntityBridge entity;
   private Logger log;
   
   public JDBCDeleteRelationsCommand(JDBCStoreManager manager) {
      this.manager = manager;
      entity = manager.getEntityBridge();

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
   public void execute(RelationData relationData) {
      if(relationData.removedRelations.size() == 0) {
         return;
      }

      String sql = createSQL(relationData);
      
      Connection con = null;
      PreparedStatement ps = null;
      JDBCCMRFieldBridge cmrField = relationData.getLeftCMRField();
      try {
         // get the connection
         DataSource dataSource = cmrField.getDataSource();
         con = dataSource.getConnection();
         
         // create the statement
         log.debug("Executing SQL: " + sql);
         ps = con.prepareStatement(sql);
         
         // set the parameters
         setParameters(ps, relationData);

         // execute statement
         int rowsAffected = ps.executeUpdate();
         log.debug("Rows affected = " + rowsAffected);
      } catch(Exception e) {
         throw new EJBException("Could not delete relations from " + 
               cmrField.getTableName(), e);
      } finally {
         JDBCUtil.safeClose(ps);
         JDBCUtil.safeClose(con);
      }
   }

   private String createSQL(RelationData relationData) {
      JDBCCMRFieldBridge left = relationData.getLeftCMRField();
      JDBCCMRFieldBridge right = relationData.getRightCMRField();
      
      StringBuffer sql = new StringBuffer();
      sql.append("DELETE FROM ");
      sql.append(left.getTableName());      
         
      sql.append(" WHERE ");
      Iterator pairs = relationData.removedRelations.iterator();
      while(pairs.hasNext()) {
         RelationPair pair = (RelationPair)pairs.next();
         sql.append("(");
            // left keys
            sql.append(SQLUtil.getWhereClause(left.getTableKeyFields()));
            sql.append(" AND ");
            // right keys
            sql.append(SQLUtil.getWhereClause(right.getTableKeyFields()));
         sql.append(")");
      
         if(pairs.hasNext()) {
            sql.append(" OR ");
         } 
      }
      
      return sql.toString();
   }
      
   private void setParameters(
         PreparedStatement ps,
         RelationData relationData) throws Exception {
      
      int index = 1;
      Iterator pairs = relationData.removedRelations.iterator();
      while(pairs.hasNext()) {
         RelationPair pair = (RelationPair)pairs.next();
         
         // left keys
         Object leftId = pair.getLeftId();
         List leftFields = relationData.getLeftCMRField().getTableKeyFields();
         for(Iterator fields=leftFields.iterator(); fields.hasNext();) {
            JDBCCMPFieldBridge field = (JDBCCMPFieldBridge)fields.next();
            index = field.setPrimaryKeyParameters(ps, index, leftId);
         }
               
         // right keys
         Object rightId = pair.getRightId();
         List rightFields = relationData.getRightCMRField().getTableKeyFields();
         for(Iterator fields=rightFields.iterator(); fields.hasNext();) {
            JDBCCMPFieldBridge field = (JDBCCMPFieldBridge)fields.next();
            index = field.setPrimaryKeyParameters(ps, index, rightId);
         }
      }
   }
}
