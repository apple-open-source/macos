/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.cmp.jdbc;

import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import javax.ejb.EJBException;
import javax.ejb.NoSuchEntityException;

import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMPFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCEntityBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMRFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCFunctionMappingMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCReadAheadMetaData;
import org.jboss.logging.Logger;

/**
 * JDBCLoadEntityCommand loads the data for an instance from the table.
 * This command implements specified eager loading. For CMP 2.x, the
 * entity can be configured to only load some of the fields, which is
 * helpful for entitys with lots of data.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="mailto:on@ibis.odessa.ua">Oleg Nitz</a>
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @author <a href="mailto:shevlandj@kpi.com.au">Joe Shevland</a>
 * @author <a href="mailto:justin@j-m-f.demon.co.uk">Justin Forder</a>
 * @author <a href="mailto:dirk@jboss.de">Dirk Zimmermann</a>
 * @author <a href="mailto:danch@nvisia.com">danch (Dan Christopherson)</a>
 * @version $Revision: 1.19.2.10 $
 */
public class JDBCLoadEntityCommand
{
   private final JDBCStoreManager manager;
   private final JDBCEntityBridge entity;
   private final Logger log;

   public JDBCLoadEntityCommand(JDBCStoreManager manager)
   {
      this.manager = manager;
      entity = manager.getEntityBridge();

      // Create the Log
      log = Logger.getLogger(
            this.getClass().getName() +
            "." +
            manager.getMetaData().getName());
   }

   /**
    * Loads entity.
    * Throws NoSuchEntityException if entity wasn't found.
    * @param ctx - entity context.
    */
   public void execute(EntityEnterpriseContext ctx)
   {
      execute(null, ctx);
   }

   /**
    * Loads entity.
    * If failIfNotFound is true and entity wasn't found then NoSuchEntityException is thrown.
    * Otherwise, if entity wasn't found, returns false.
    * If entity was loaded successfully return true.
    * @param ctx - entity context;
    * @param failIfNotFound - whether to fail if entity wasn't found;
    * @return true if entity was loaded, false - otherwise.
    */
   public boolean execute(EntityEnterpriseContext ctx, boolean failIfNotFound)
   {
      return execute(null, ctx, failIfNotFound);
   }

   /**
    * Loads entity or required field. If entity not found throws NoSuchEntityException.
    * @param requiredField - required field or null;
    * @param ctx - the corresponding context;
    */
   public void execute(JDBCCMPFieldBridge requiredField, EntityEnterpriseContext ctx)
   {
      execute(requiredField, ctx, true);
   }

   /**
    * Loads entity or required field.
    * If failIfNotFound is set to true, then NoSuchEntityException is thrown if the
    * entity wasn't found.
    * If failIfNotFound is false then if the entity wasn't found returns false,
    * if the entity was loaded successfully, returns true.
    * @param requiredField - required field;
    * @param ctx - entity context;
    * @param failIfNotFound - whether to fail if entity wasn't loaded.
    * @return true if entity was loaded, false - otherwise.
    */
   public boolean execute(
         JDBCCMPFieldBridge requiredField,
         EntityEnterpriseContext ctx,
         boolean failIfNotFound) {

      // load the instance primary key fields into the context
      entity.injectPrimaryKeyIntoInstance(ctx, ctx.getId());

      // get the read ahead cache
      ReadAheadCache readAheadCache = manager.getReadAheadCache();

      // load any preloaded fields into the context
      readAheadCache.load(ctx);

      // get the finder results associated with this context, if it exists
      ReadAheadCache.EntityReadAheadInfo info =
         readAheadCache.getEntityReadAheadInfo(ctx.getId());

      // determine the fields to load
      List loadFields = getLoadFields(requiredField, info.getReadAhead(), ctx);

      // if no there are not load fields return
      if(loadFields.size() == 0) {
         return true;
      }

      // get the keys to load
      List loadKeys = info.getLoadKeys();

      // generate the sql
      String sql = getSQL(loadFields, loadKeys.size());

      Connection con = null;
      PreparedStatement ps = null;
      try {
         // get the connection
         con = entity.getDataSource().getConnection();

         // create the statement
         if(log.isDebugEnabled()) {
            log.debug("Executing SQL: " + sql);
         }
         ps = con.prepareStatement(sql);

         // Set the fetch size of the statement
         if(manager.getEntityBridge().getFetchSize() > 0) {
            ps.setFetchSize(manager.getEntityBridge().getFetchSize());
         }

         // set the parameters

         int paramIndex = 1;
         for(Iterator iter = loadKeys.iterator(); iter.hasNext();) {
            paramIndex = entity.setPrimaryKeyParameters(
                  ps, paramIndex, iter.next());
         }

         // execute statement
         ResultSet rs = ps.executeQuery();

         // load results
         boolean mainEntityLoaded = false;
         Object[] ref = new Object[1];
         while(rs.next()) {
            // reset the column index for this row
            int index = 1;

            // ref must be reset to null before load
            ref[0] = null;

            // if we are loading more then one entity, load the pk from the row
            Object pk = null;
            if(loadKeys.size() > 1) {
               // load the pk
               index = entity.loadPrimaryKeyResults(rs, index, ref);
               pk = ref[0];
            }

            // is this the main entity or a preload entity
            if(loadKeys.size()==1 || pk.equals(ctx.getId())) {
               // main entity; load the values into the context
               for(Iterator iter = loadFields.iterator(); iter.hasNext();) {
                  JDBCFieldBridge field = (JDBCFieldBridge)iter.next();
                  index = field.loadInstanceResults(rs, index, ctx);
                  field.setClean(ctx);
               }
               mainEntityLoaded = true;
            } else {
               // preload entity; load the values into the read ahead cahce
               for(Iterator iter = loadFields.iterator(); iter.hasNext();) {
                  JDBCFieldBridge field = (JDBCFieldBridge)iter.next();

                  // ref must be reset to null before load
                  ref[0] = null;

                  // load the result of the field
                  index = field.loadArgumentResults(rs, index, ref);

                  // cache the field value
                  readAheadCache.addPreloadData(pk, field, ref[0]);
               }
            }
         }

         // did we load the main results
         if(!mainEntityLoaded)
         {
            if(failIfNotFound)
               throw new NoSuchEntityException("Entity not found: primaryKey=" + ctx.getId());
            else
               return false;
         }
         else
            return true;
      } catch(EJBException e) {
         throw e;
      } catch(Exception e) {
         throw new EJBException("Load failed", e);
      } finally {
         JDBCUtil.safeClose(ps);
         JDBCUtil.safeClose(con);
      }
   }

   private String getSQL(List loadFields, int keyCount) {
      //
      // column names clause
      StringBuffer columnNamesClause = new StringBuffer();
      // if we are loading more then one entity we need to add the primry
      // key to the load fields to match up the results with the correct
      // entity.
      if(keyCount > 1) {
         columnNamesClause.append(SQLUtil.getColumnNamesClause(
                  entity.getPrimaryKeyFields()));
         columnNamesClause.append(",");
      }
      columnNamesClause.append(SQLUtil.getColumnNamesClause(loadFields));

      //
      // table name clause
      String tableName = entity.getTableName();

      //
      // where clause
      String pkWhere = SQLUtil.getWhereClause(entity.getPrimaryKeyFields());
      StringBuffer whereClause = new StringBuffer(
            (pkWhere.length() + 6) * keyCount + 4);
      for(int i=0; i<keyCount; i++) {
         if(i > 0) {
            whereClause.append(" OR ");
         }
         whereClause.append("(").append(pkWhere).append(")");
      }

      //
      // assemble pieces into final statement
      if(entity.getMetaData().hasRowLocking()) {
         JDBCFunctionMappingMetaData rowLocking =
               manager.getMetaData().getTypeMapping().getRowLockingTemplate();
         if(rowLocking == null) {
            throw new IllegalStateException("row-locking is not allowed for " +
                  "this type of datastore");
         } else {
            String[] args = new String[] {
               columnNamesClause.toString(),
               tableName,
               whereClause.toString()};
            return rowLocking.getFunctionSql(args);
         }
      } else {
         StringBuffer sql = new StringBuffer(
               7 + columnNamesClause.length() +
               6 + tableName.length() +
               7 + whereClause.length());

         sql.append("SELECT ").append(columnNamesClause.toString());
         sql.append(" FROM ").append(tableName);
         sql.append(" WHERE ").append(whereClause.toString());
         return sql.toString();
      }

   }

   private List getLoadFields(
         JDBCCMPFieldBridge requiredField,
         JDBCReadAheadMetaData readahead,
         EntityEnterpriseContext ctx) {

      // get the load fields
      ArrayList loadFields = new ArrayList(entity.getFields().size());
      if(requiredField == null) {

         if(readahead != null && !readahead.isNone()) {
            if(log.isTraceEnabled()) {
               log.trace("Eager-load for entity: readahead=" +  readahead);
            }
            loadFields.addAll(
                  entity.getLoadGroup(readahead.getEagerLoadGroup()));
         } else {
            if(log.isTraceEnabled()) {
               log.trace("Default eager-load for entity: readahead=" +
                     readahead);
            }
            loadFields.addAll(entity.getEagerLoadFields());
         }
      } else {
         loadFields.add(requiredField);
         for(Iterator groups = entity.getLazyLoadGroups(); groups.hasNext();) {
            List group = (List)groups.next();
            if(group.contains(requiredField)) {
               for(Iterator fields = group.iterator(); fields.hasNext();) {
                  JDBCFieldBridge field = (JDBCFieldBridge)fields.next();
                  if(!loadFields.contains(field)) {
                     loadFields.add(field);
                  }
               }
            }
         }
      }

      // remove any field that is a member of the primary key
      // or has not timed out or is already loaded
      for(Iterator fields = loadFields.iterator(); fields.hasNext();) {
         JDBCFieldBridge field = (JDBCFieldBridge)fields.next();

         // the field removed from loadFields if:
         // - it is a primary key member (should already be loaded)
         // - it is already loaded
         // - it is a read-only _already_loaded_ field that isn't timed out yet
         // - it is a CMR field with a foreign key mapped to a pk
         if(field.isPrimaryKeyMember()
            || field.isLoaded( ctx )
               && (!field.isReadOnly() || !field.isReadTimedOut(ctx))
            || field instanceof JDBCCMRFieldBridge
               && ((JDBCCMRFieldBridge)field).allFkFieldsMappedToPkFields()) {
            fields.remove();
         }
      }
      return loadFields;
   }
}
