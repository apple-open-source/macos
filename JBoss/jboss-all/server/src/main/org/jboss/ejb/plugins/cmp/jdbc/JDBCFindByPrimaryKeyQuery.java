/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc;

import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCEntityBridge;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCQueryMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCReadAheadMetaData;
import org.jboss.ejb.EntityEnterpriseContext;

import javax.ejb.FinderException;
import java.util.Collection;
import java.util.Collections;
import java.lang.reflect.Method;

/**
 * JDBCBeanExistsCommand is a JDBC query that checks if an id exists
 * in the database.  This is used by the create and findByPrimaryKey
 * code.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @author <a href="mailto:justin@j-m-f.demon.co.uk">Justin Forder</a>
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 * @version $Revision: 1.4.4.13 $
 */
public final class JDBCFindByPrimaryKeyQuery extends JDBCAbstractQueryCommand
{
   private JDBCStoreManager manager;
   private boolean rowLocking;

   public JDBCFindByPrimaryKeyQuery(JDBCStoreManager man, JDBCQueryMetaData q)
   {
      super(man, q);
      this.manager = man;
      rowLocking = manager.getMetaData().hasRowLocking();

      JDBCEntityBridge entity = manager.getEntityBridge();

      // set the preload fields
      JDBCReadAheadMetaData readAhead = q.getReadAhead();
      if (readAhead.isOnFind())
      {
         setEagerLoadGroup(readAhead.getEagerLoadGroup());
      }

      // generate the sql
      StringBuffer sql = new StringBuffer(300);
      sql.append(SQLUtil.SELECT);

      // put pk fields first
      StringBuffer forUpdate = null;
      if (getEagerLoadMask() != null)
      {
         StringBuffer columnNamesClause = new StringBuffer(200);
         SQLUtil.getColumnNamesClause(entity.getPrimaryKeyFields(), entity.getTableName(), columnNamesClause);
         columnNamesClause.append(SQLUtil.COMMA);
         SQLUtil.getColumnNamesClause(entity.getTableFields(), getEagerLoadMask(), entity.getTableName(), columnNamesClause);

         if (rowLocking)
            forUpdate = new StringBuffer(" FOR UPDATE OF ").append(columnNamesClause);


         preloadableCmrs = JDBCAbstractQueryCommand.getPreloadableCmrs(getEagerLoadMask(), manager);
         deepCmrs = null;
         if (preloadableCmrs != null && preloadableCmrs.length > 0)
         {
            deepCmrs = JDBCAbstractQueryCommand.deepPreloadableCmrs(preloadableCmrs);
            StringBuffer[] ref = {forUpdate};
            cmrColumnNames(deepCmrs, columnNamesClause, ref);
            forUpdate = ref[0];
         }
         sql.append(columnNamesClause)
                 .append(SQLUtil.FROM)
                 .append(entity.getTableName());

         if (deepCmrs != null)
         {
            generateCmrOuterJoin(deepCmrs, entity.getTableName(), sql);
         }
      }
      else
      {
         SQLUtil.getColumnNamesClause(entity.getPrimaryKeyFields(), sql)
                 .append(SQLUtil.FROM)
                 .append(entity.getTableName());
      }

      sql.append(SQLUtil.WHERE);
      SQLUtil.getWhereClause(entity.getPrimaryKeyFields(), sql);
      if (forUpdate != null) sql.append(forUpdate);

      setSQL(sql.toString());
      setParameterList(QueryParameter.createPrimaryKeyParameters(0, entity));
   }

   public Collection execute(Method finderMethod, Object[] args, EntityEnterpriseContext ctx)
           throws FinderException
   {
      // Check in readahead cache.
      if (rowLocking && manager.getReadAheadCache().getPreloadDataMap(args[0], false) != null)
      {
         return Collections.singletonList(args[0]);
      }
      return super.execute(finderMethod, args, ctx);
   }
}
