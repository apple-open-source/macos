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
 * Inserts relations into a relation table.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
 * @version $Revision: 1.11.4.7 $
 */
public final class JDBCInsertRelationsCommand {
   private final Logger log;
    
   public JDBCInsertRelationsCommand(JDBCStoreManager manager) {
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
   
   protected static String getSQL(RelationData relationData) {
      JDBCCMRFieldBridge left = relationData.getLeftCMRField();
      JDBCCMRFieldBridge right = relationData.getRightCMRField();
      
      StringBuffer sql = new StringBuffer(200);
      sql.append(SQLUtil.INSERT_INTO).append(left.getTableName());

      sql.append('(');
         SQLUtil.getColumnNamesClause(left.getTableKeyFields(), sql);
      sql.append(SQLUtil.COMMA);
         SQLUtil.getColumnNamesClause(right.getTableKeyFields(), sql);
      sql.append(')');

      sql.append(SQLUtil.VALUES).append('(');
            SQLUtil.getValuesClause(left.getTableKeyFields(), sql);
            sql.append(SQLUtil.COMMA);
            SQLUtil.getValuesClause(right.getTableKeyFields(), sql);
      sql.append(')');
      return sql.toString();
   }
      
   protected static void setParameters(PreparedStatement ps,
                                RelationData relationData,
                                RelationPair pair)
   {
      int index = 1;

      // left keys
      Object leftId = pair.getLeftId();
      JDBCCMPFieldBridge[] leftFields = relationData.getLeftCMRField().getTableKeyFields();
      for(int i = 0; i < leftFields.length; ++i)
         index = leftFields[i].setPrimaryKeyParameters(ps, index, leftId);

      // right keys
      Object rightId = pair.getRightId();
      JDBCCMPFieldBridge[] rightFields = relationData.getRightCMRField().getTableKeyFields();
      for(int i = 0; i < rightFields.length; ++i)
         index = rightFields[i].setPrimaryKeyParameters(ps, index, rightId);
   }
}
