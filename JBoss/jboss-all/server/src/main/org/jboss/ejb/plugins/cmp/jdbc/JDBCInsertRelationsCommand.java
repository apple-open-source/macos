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

import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMPFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMRFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCEntityBridge;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCRelationMetaData;
import org.jboss.logging.Logger;

/**
 * Inserts relations into a relation table.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.11.4.1 $
 */
public class JDBCInsertRelationsCommand {
   protected JDBCStoreManager manager;
   protected Logger log;
    
   protected JDBCEntityBridge entity;
   
   public JDBCInsertRelationsCommand(JDBCStoreManager manager) {
      this.manager = manager;
      this.entity = manager.getEntityBridge();

      this.log = Logger.getLogger(
            this.getClass().getName() + 
            "." + 
            manager.getMetaData().getName());
   }
   
   public void execute(RelationData relationData) {
      if(relationData.addedRelations.size() == 0) {
         return;
      }
      
      Connection con = null;
      PreparedStatement ps = null;
      
      JDBCCMRFieldBridge cmrField = relationData.getLeftCMRField();
      try {
         // get the sql
         String sql = getSQL(relationData);
         boolean debug = log.isDebugEnabled();
         if(debug)
            log.debug("Executing SQL: " + sql);

         // get the connection
         DataSource dataSource = cmrField.getDataSource();
         con = dataSource.getConnection();
         
         // get a prepared statement
         ps = con.prepareStatement(sql);
         
         Iterator pairs = relationData.addedRelations.iterator();
         while(pairs.hasNext()) {
            RelationPair pair = (RelationPair)pairs.next();
            
            // set the parameters
            setParameters(ps, relationData, pair);
         
            int rowsAffected = ps.executeUpdate();
         
            if(debug)
               log.debug("Rows affected = " + rowsAffected);
         }
      } catch(Exception e) {
         throw new EJBException("Could insert relations into " +
               cmrField.getTableName(), e);
      } finally {
         JDBCUtil.safeClose(ps);
         JDBCUtil.safeClose(con);
      }
   }
   
   protected String getSQL(RelationData relationData) throws Exception {
      JDBCCMRFieldBridge left = relationData.getLeftCMRField();
      JDBCCMRFieldBridge right = relationData.getRightCMRField();
      
      StringBuffer sql = new StringBuffer(200);
      sql.append("INSERT INTO ").append(
           left.getTableName());      

      sql.append(" (");
            sql.append(SQLUtil.getColumnNamesClause(left.getTableKeyFields()));
            sql.append(", ");
            sql.append(SQLUtil.getColumnNamesClause(right.getTableKeyFields()));
      sql.append(")");

      sql.append(" VALUES (");
            sql.append(SQLUtil.getValuesClause(left.getTableKeyFields()));
            sql.append(", ");
            sql.append(SQLUtil.getValuesClause(right.getTableKeyFields()));
      sql.append(")");      
      return sql.toString();
   }
      
   protected void setParameters(
         PreparedStatement ps,
         RelationData relationData,
         RelationPair pair) throws Exception {

      int index = 1;

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
