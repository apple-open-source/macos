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
import java.util.List;
import javax.ejb.EJBException;
import javax.ejb.NoSuchEntityException;

import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMPFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCEntityBridge;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCFunctionMappingMetaData;
import org.jboss.logging.Logger;

/**
 * JDBCLoadEntityCommand loads the data for an instance from the table.
 * This command implements specified eager loading. For CMP 2.x, the
 * entity can be configured to only load some of the fields, which is
 * helpful for entitys with lots of data.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="mailto:on@ibis.odessa.ua">Oleg Nitz</a>
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard �berg</a>
 * @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @author <a href="mailto:shevlandj@kpi.com.au">Joe Shevland</a>
 * @author <a href="mailto:justin@j-m-f.demon.co.uk">Justin Forder</a>
 * @author <a href="mailto:dirk@jboss.de">Dirk Zimmermann</a>
 * @author <a href="mailto:danch@nvisia.com">danch (Dan Christopherson)</a>
 * @author <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
 * @version $Revision: 1.19.2.20 $
 */
public final class JDBCLoadEntityCommand
{
   private final JDBCStoreManager manager;
   private final JDBCEntityBridge entity;
   private final Logger log;
   private final boolean rawLocking;

   public JDBCLoadEntityCommand(JDBCStoreManager manager)
   {
      this.manager = manager;
      entity = manager.getEntityBridge();
      rawLocking = entity.getMetaData().hasRowLocking();

      // Create the Log
      log = Logger.getLogger(
              this.getClass().getName() +
              "." +
              manager.getMetaData().getName());
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
   private boolean execute(JDBCCMPFieldBridge requiredField,
                           EntityEnterpriseContext ctx,
                           boolean failIfNotFound)
   {
      // load the instance primary key fields into the context
      Object id = ctx.getId();
      // TODO: when exactly do I need to do the following?
      entity.injectPrimaryKeyIntoInstance(ctx, id);

      // get the read ahead cache
      ReadAheadCache readAheadCache = manager.getReadAheadCache();

      // load any preloaded fields into the context
      //log.info("###### calling loadFromLoadEntity " + entity.getMetaData().getName() + " pk = " + ctx.getId() +  "requiredField: " + requiredField + " thread=" + Thread.currentThread());
      readAheadCache.load(ctx);
      //log.info("###### done calling loadFromLoadEntity " + entity.getMetaData().getName() + " pk = " + ctx.getId() + " thread=" + Thread.currentThread());

      // get the finder results associated with this context, if it exists
      ReadAheadCache.EntityReadAheadInfo info = readAheadCache.getEntityReadAheadInfo(id);

      // determine the fields to load
      JDBCEntityBridge.FieldIterator loadIter = entity.getLoadIterator(requiredField, info.getReadAhead(), ctx);
      if(!loadIter.hasNext())
         return true;

      //log.info("###### ejbLoading entity: " + entity.getMetaData().getName() + " thread=" + Thread.currentThread());
      // get the keys to load
      List loadKeys = info.getLoadKeys();

      // generate the sql
      String sql = (rawLocking ? getRawLockingSQL(loadIter, loadKeys.size()) : getSQL(loadIter, loadKeys.size()));

      Connection con = null;
      PreparedStatement ps = null;
      ResultSet rs = null;
      try
      {
         // create the statement
         if (log.isDebugEnabled())
         {
            log.debug("Executing SQL: " + sql);
         }

         // get the connection
         con = entity.getDataSource().getConnection();
         ps = con.prepareStatement(sql);

         // Set the fetch size of the statement
         if (manager.getEntityBridge().getFetchSize() > 0)
         {
            ps.setFetchSize(manager.getEntityBridge().getFetchSize());
         }

         // set the parameters
         int paramIndex = 1;
         for (int i = 0; i < loadKeys.size(); i++)
         {
            paramIndex = entity.setPrimaryKeyParameters(ps, paramIndex, loadKeys.get(i));
         }

         // execute statement
         rs = ps.executeQuery();

         // load results
         boolean mainEntityLoaded = false;
         Object[] ref = new Object[1];
         while (rs.next())
         {
            // reset the column index for this row
            int index = 1;

            // ref must be reset to null before load
            ref[0] = null;

            // if we are loading more then one entity, load the pk from the row
            Object pk = null;
            if (loadKeys.size() > 1)
            {
               // load the pk
               index = entity.loadPrimaryKeyResults(rs, index, ref);
               pk = ref[0];
            }

            // is this the main entity or a preload entity
            if (loadKeys.size() == 1 || pk.equals(id))
            {
               // main entity; load the values into the context
               loadIter.reset();
               while(loadIter.hasNext())
               {
                  JDBCCMPFieldBridge field = loadIter.next();
                  index = field.loadInstanceResults(rs, index, ctx);
                  field.setClean(ctx);
               }
               mainEntityLoaded = true;
            }
            else
            {
               // preload entity; load the values into the read ahead cahce
               loadIter.reset();
               while(loadIter.hasNext())
               {
                  JDBCCMPFieldBridge field = loadIter.next();
                  // ref must be reset to null before load
                  ref[0] = null;

                  // load the result of the field
                  index = field.loadArgumentResults(rs, index, ref);

                  // cache the field value
                  readAheadCache.addPreloadData(pk, field, ref[0]);
               }
            }
         }

         // clear LOAD_REQUIRED flag
         loadIter.removeAll();

         // did we load the main results
         if (!mainEntityLoaded)
         {
            if (failIfNotFound)
               throw new NoSuchEntityException("Entity not found: primaryKey=" + ctx.getId());
            else
               return false;
         }
         else
            return true;
      }
      catch (EJBException e)
      {
         throw e;
      }
      catch (Exception e)
      {
         throw new EJBException("Load failed", e);
      }
      finally
      {
         JDBCUtil.safeClose(rs);
         JDBCUtil.safeClose(ps);
         JDBCUtil.safeClose(con);
      }
   }

   private String getSQL(JDBCEntityBridge.FieldIterator loadIter, int keyCount)
   {
      StringBuffer sql = new StringBuffer(250);
      sql.append(SQLUtil.SELECT);

      // if we are loading more then one entity we need to add the primry
      // key to the load fields to match up the results with the correct entity.
      JDBCFieldBridge[] primaryKeyFields = entity.getPrimaryKeyFields();
      if (keyCount > 1)
      {
         SQLUtil.getColumnNamesClause(primaryKeyFields, sql);
         sql.append(SQLUtil.COMMA);
      }
      SQLUtil.getColumnNamesClause(loadIter, sql);
      sql.append(SQLUtil.FROM)
              .append(entity.getTableName())
              .append(SQLUtil.WHERE);

      //
      // where clause
      String pkWhere = SQLUtil.getWhereClause(primaryKeyFields, new StringBuffer(50)).toString();
      sql.append('(').append(pkWhere).append(')');
      for (int i = 1; i < keyCount; i++)
      {
         sql.append(SQLUtil.OR).append('(').append(pkWhere).append(')');
      }

      return sql.toString();
   }

   private String getRawLockingSQL(JDBCEntityBridge.FieldIterator loadIter, int keyCount)
   {
      //
      // column names clause
      StringBuffer columnNamesClause = new StringBuffer(250);
      // if we are loading more then one entity we need to add the primry
      // key to the load fields to match up the results with the correct
      // entity.
      if (keyCount > 1)
      {
         SQLUtil.getColumnNamesClause(entity.getPrimaryKeyFields(), columnNamesClause);
         columnNamesClause.append(SQLUtil.COMMA);
      }

      SQLUtil.getColumnNamesClause(loadIter, columnNamesClause);

      //
      // table name clause
      String tableName = entity.getTableName();

      //
      // where clause
      String whereClause = SQLUtil.
              getWhereClause(entity.getPrimaryKeyFields(), new StringBuffer(50)).toString();
      if (keyCount > 0)
      {
         StringBuffer sb = new StringBuffer((whereClause.length() + 6) * keyCount + 4);
         for (int i = 0; i < keyCount; i++)
         {
            if (i > 0)
               sb.append(SQLUtil.OR);
            sb.append('(').append(whereClause).append(')');
         }
         whereClause = sb.toString();
      }

      JDBCFunctionMappingMetaData rowLocking =
              manager.getMetaData().getTypeMapping().getRowLockingTemplate();
      if (rowLocking == null)
      {
         throw new IllegalStateException(
                 "row-locking is not allowed for this type of datastore");
      }
      else
      {
         String[] args = new String[]{
            columnNamesClause.toString(),
            tableName,
            whereClause
         };
         return rowLocking.getFunctionSql(args, new StringBuffer(300)).toString();
      }
   }

   /*
   private ArrayList getDeepSQL(long loadMask, int keyCount, StringBuffer theSql)
   {
      // table name clause
      String tableName = entity.getTableName();

      //
      // column names clause
      StringBuffer columnNamesClause = new StringBuffer(250);
      StringBuffer leftJoin = new StringBuffer(250);
      StringBuffer forUpdate = null;
      if (rowLocking)
      {
         forUpdate = new StringBuffer(" FOR UPDATE OF ");
      }
      // if we are loading more then one entity we need to add the primry
      // key to the load fields to match up the results with the correct
      // entity.
      if (keyCount > 1)
      {
         SQLUtil.getColumnNamesClause(entity.getPrimaryKeyFields(), tableName, columnNamesClause);
         columnNamesClause.append(SQLUtil.COMMA);
      }

      SQLUtil.getColumnNamesClause(tableFields, loadMask, tableName, columnNamesClause);
      if (rowLocking) forUpdate.append(columnNamesClause);


      JDBCCMRFieldBridge[] cmrs = entity.getCMRFields();
      ArrayList polledCmrs = null;
      if (cmrs != null)
      {
         for (int i = 0; i < cmrs.length; i++)
         {
            if (cmrs[i].getMetaData().isDeepReadAhead() == false) continue;
            JDBCCMP2xFieldBridge[] cmrTableFields = cmrs[i].getForeignKeyFields();
            if (cmrTableFields == null)
            {
               continue;
            }
            boolean found = false;
            for (int k = 0; k < cmrTableFields.length && found == false; k++)
            {
               JDBCCMPFieldBridge f = cmrTableFields[k].getCmpFieldIAmMappedTo();
               for (int l = 0; l < tableFields.length; l++)
               {
                  if (tableFields[l] == f && (loadMask & tableFields[l].getFieldMask()) != 0)
                  {
                     found = true;
                     break;
                  }
               }
               for (int l = 0; l < entity.getPrimaryKeyFields().length; l++)
               {
                  if (entity.getPrimaryKeyFields()[l] == f)
                  {
                     found = true;
                     break;
                  }

               }
            }
            if (found)
            {
               if (polledCmrs == null) polledCmrs = new ArrayList(cmrs.length);
               polledCmrs.add(cmrs[i]);
               StringBuffer cmrClause = new StringBuffer(100);
               columnNamesClause.append(SQLUtil.COMMA);
               SQLUtil.getColumnNamesClause(cmrs[i].getRelatedJDBCEntity().getTableFields(), cmrs[i].getFieldName(), cmrClause);
               if (cmrs[i].getRelatedJDBCEntity().getMetaData().hasRowLocking())
               {
                  if (forUpdate == null)
                     forUpdate = new StringBuffer(" FOR UPDATE OF ");
                  else
                     forUpdate.append(", ");
                  forUpdate.append(cmrClause);
               }
               columnNamesClause.append(cmrClause);
               JDBCEntityBridge childEntity = cmrs[i].getRelatedJDBCEntity();


               leftJoin.append(SQLUtil.LEFT_OUTER_JOIN)
                       .append(childEntity.getTableName())
                       .append(' ')
                       .append(cmrs[i].getFieldName())
                       .append(SQLUtil.ON);
               SQLUtil.getJoinClause(cmrs[i], tableName, cmrs[i].getFieldName(), leftJoin);
               leftJoin.append(" ");
            }
         }
      }
      //
      // where clause
      String whereClause = SQLUtil.getWhereClause(entity.getPrimaryKeyFields(), new StringBuffer(50)).toString();
      if (keyCount > 0)
      {
         StringBuffer sb = new StringBuffer((whereClause.length() + 6) * keyCount + 4);
         for (int i = 0; i < keyCount; i++)
         {
            if (i > 0)
               sb.append(SQLUtil.OR);
            sb.append('(').append(whereClause).append(')');
         }
         whereClause = sb.toString();
      }

      theSql.append("SELECT ");
      theSql.append(columnNamesClause.toString());
      theSql.append(" FROM ").append(tableName).append(" ").append(leftJoin).append(" WHERE ").append(whereClause);
      if (forUpdate != null) theSql.append(forUpdate);

      return polledCmrs;
   }
   */
}
