/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.cmp.jdbc;

import java.lang.reflect.Method;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.StringTokenizer;
import javax.ejb.FinderException;

import org.jboss.deployment.DeploymentException;
import org.jboss.ejb.EJBProxyFactory;
import org.jboss.ejb.EntityContainer;
import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.ejb.LocalProxyFactory;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMPFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCEntityBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMRFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMP2xFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCQueryMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCReadAheadMetaData;
import org.jboss.ejb.plugins.cmp.ejbql.SelectFunction;
import org.jboss.logging.Logger;

/**
 * Abstract superclass of finder commands that return collections.
 * Provides the handleResult() implementation that these all need.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard �berg</a>
 * @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @author <a href="mailto:shevlandj@kpi.com.au">Joe Shevland</a>
 * @author <a href="mailto:justin@j-m-f.demon.co.uk">Justin Forder</a>
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 *
 * @version $Revision: 1.12.2.21 $
 */
public abstract class JDBCAbstractQueryCommand implements JDBCQueryCommand
{
   // todo: get rid of it
   private static final String FINDER_PREFIX = "find";
   private JDBCStoreManager manager;
   private JDBCQueryMetaData queryMetaData;
   protected Logger log;

   private JDBCStoreManager selectManager;
   private JDBCEntityBridge selectEntity;
   private JDBCCMPFieldBridge selectField;
   private SelectFunction selectFunction;
   private boolean[] eagerLoadMask;
   private String eagerLoadGroup;
   private String sql;
   private int offsetParam;
   private int offsetValue;
   private int limitParam;
   private int limitValue;
   private List parameters = new ArrayList(0);
   protected JDBCCMRFieldBridge[] preloadableCmrs = null;
   protected ArrayList deepCmrs = null;

   public JDBCAbstractQueryCommand(JDBCStoreManager manager, JDBCQueryMetaData q)
   {
      this.manager = manager;
      this.log = Logger.getLogger(
              this.getClass().getName() +
              "." +
              manager.getMetaData().getName() +
              "#" +
              q.getMethod().getName());

      queryMetaData = q;
//      setDefaultOffset(q.getOffsetParam());
//      setDefaultLimit(q.getLimitParam());
      setSelectEntity(manager.getEntityBridge());
   }

   public void setOffsetValue(int offsetValue)
   {
      this.offsetValue = offsetValue;
   }

   public void setLimitValue(int limitValue)
   {
      this.limitValue = limitValue;
   }

   public void setOffsetParam(int offsetParam)
   {
      this.offsetParam = offsetParam;
   }

   public void setLimitParam(int limitParam)
   {
      this.limitParam = limitParam;
   }

   public Collection execute(Method finderMethod, Object[] args, EntityEnterpriseContext ctx)
           throws FinderException
   {
      int offset = toInt(args, offsetParam, offsetValue);
      int limit = toInt(args, limitParam, limitValue);
      return execute(args, offset, limit);
   }

   private static int toInt(Object[] params, int paramNumber, int defaultValue)
   {
      if (paramNumber == 0)
         return defaultValue;
      Integer arg = (Integer) params[paramNumber - 1];
      return arg.intValue();
   }

   private Collection execute(Object[] args, int offset, int limit)
           throws FinderException
   {
      ReadAheadCache selectReadAheadCache = null;
      if (selectEntity != null)
      {
         selectReadAheadCache = selectManager.getReadAheadCache();
      }

      List results = new ArrayList();

      Connection con = null;
      PreparedStatement ps = null;
      ResultSet rs = null;
      try
      {
         // create the statement
         if (log.isDebugEnabled())
         {
            log.debug("Executing SQL: " + sql);
            if (limit != 0 || offset != 0)
            {
               log.debug("Query offset=" + offset + ", limit=" + limit);
            }
         }

         // get the connection
         con = manager.getEntityBridge().getDataSource().getConnection();
         ps = con.prepareStatement(sql);

         // Set the fetch size of the statement
         if (manager.getEntityBridge().getFetchSize() > 0)
         {
            ps.setFetchSize(manager.getEntityBridge().getFetchSize());
         }

         // set the parameters
         for (int i = 0; i < parameters.size(); i++)
         {
            QueryParameter parameter = (QueryParameter) parameters.get(i);
            parameter.set(log, ps, i + 1, args);
         }

         // execute statement
         rs = ps.executeQuery();

         // skip 'offset' results
         int count = offset;
         while (count > 0 && rs.next())
         {
            count--;
         }

         count = limit;

         // load the results
         if (selectEntity != null)
         {
            Object[] ref = new Object[1];
            while ((limit == 0 || count-- > 0) && rs.next())
            {
               int index = 1;

               // get the pk
               index = selectEntity.loadPrimaryKeyResults(rs, index, ref);
               Object pk = ref[0];
               results.add(ref[0]);

               // read the preload fields
               if (eagerLoadMask != null)
               {
                  JDBCCMPFieldBridge[] tableFields = selectEntity.getTableFields();
                  for (int i = 0; i < eagerLoadMask.length; i++)
                  {
                     if (eagerLoadMask[i])
                     {
                        JDBCCMPFieldBridge field = tableFields[i];
                        ref[0] = null;

                        // read the value and store it in the readahead cache
                        index = field.loadArgumentResults(rs, index, ref);
                        selectReadAheadCache.addPreloadData(pk, field, ref[0]);
                     }
                  }
                  if (deepCmrs != null)
                  {
                     index = preloadCmrs(deepCmrs, rs, index);
                  }
               }
            }
         }
         else if (selectField != null)
         {
            // load the field
            Object[] valueRef = new Object[1];
            while ((limit == 0 || count-- > 0) && rs.next())
            {
               valueRef[0] = null;
               selectField.loadArgumentResults(rs, 1, valueRef);
               results.add(valueRef[0]);
            }
         }
         else
         {
            while (rs.next())
               results.add(selectFunction.readResult(rs));
         }

         if (log.isDebugEnabled() && limit != 0 && count == 0)
         {
            log.debug("Query result was limited to " + limit + " row(s)");
         }
      }
      catch (Exception e)
      {
         log.debug("Find failed", e);
         throw new FinderException("Find failed: " + e);
      }
      finally
      {
         JDBCUtil.safeClose(rs);
         JDBCUtil.safeClose(ps);
         JDBCUtil.safeClose(con);
      }

      // If we were just selecting a field, we're done.
      if (selectField != null || selectFunction != null)
      {
         return results;
      }

      // add the results list to the cache
      JDBCReadAheadMetaData readAhead = queryMetaData.getReadAhead();
      selectReadAheadCache.addFinderResults(results, readAhead);

      // If this is a finder, we're done.
      if (queryMetaData.getMethod().getName().startsWith(FINDER_PREFIX))
      {
         return results;
      }

      // This is an ejbSelect, so we need to convert the pks to real ejbs.
      EntityContainer selectContainer = selectManager.getContainer();
      if (queryMetaData.isResultTypeMappingLocal())
      {
         LocalProxyFactory localFactory = selectContainer.getLocalProxyFactory();
         return localFactory.getEntityLocalCollection(results);
      }
      else
      {
         EJBProxyFactory factory = selectContainer.getProxyFactory();
         return factory.getEntityCollection(results);
      }
   }

   protected Logger getLog()
   {
      return log;
   }

   protected void setSQL(String sql)
   {
      this.sql = sql;
      if(log.isDebugEnabled())
      {
         log.debug("SQL: " + sql);
      }
   }

   protected void setParameterList(List p)
   {
      for (int i = 0; i < p.size(); i++)
      {
         if (!(p.get(i) instanceof QueryParameter))
         {
            throw new IllegalArgumentException("Element " + i + " of list " +
                    "is not an instance of QueryParameter, but " +
                    p.get(i).getClass().getName());
         }
      }
      parameters = new ArrayList(p);
   }

   protected JDBCEntityBridge getSelectEntity()
   {
      return selectEntity;
   }

   protected void setSelectEntity(JDBCEntityBridge selectEntity)
   {
      this.selectField = null;
      this.selectFunction = null;
      this.selectEntity = selectEntity;
      this.selectManager = selectEntity.getManager();
   }

   protected JDBCCMPFieldBridge getSelectField()
   {
      return selectField;
   }

   protected void setSelectField(JDBCCMPFieldBridge selectField)
   {
      this.selectEntity = null;
      this.selectFunction = null;
      this.selectField = selectField;
      this.selectManager = selectField.getManager();
   }

   protected void setSelectFunction(SelectFunction func, JDBCStoreManager manager)
   {
      this.selectEntity = null;
      this.selectField = null;
      this.selectFunction = func;
      this.selectManager = manager;
   }

   protected void setEagerLoadGroup(String eagerLoadGroup)
   {
      this.eagerLoadGroup = eagerLoadGroup;
      this.eagerLoadMask = selectEntity.getLoadGroupMask(eagerLoadGroup);
   }

   protected String getEagerLoadGroup()
   {
      return eagerLoadGroup;
   }

   protected boolean[] getEagerLoadMask()
   {
      return this.eagerLoadMask;
   }

   /**
    * Replaces the parameters in the specifiec sql with question marks, and
    * initializes the parameter setting code. Parameters are encoded in curly
    * brackets use a zero based index.
    * @param sql the sql statement that is parsed for parameters
    * @return the original sql statement with the parameters replaced with a
    *    question mark
    * @throws DeploymentException if a error occures while parsing the sql
    */
   protected String parseParameters(String sql) throws DeploymentException
   {
      StringBuffer sqlBuf = new StringBuffer();
      ArrayList params = new ArrayList();

      // Replace placeholders {0} with ?
      if (sql != null)
      {
         sql = sql.trim();

         StringTokenizer tokens = new StringTokenizer(sql, "{}", true);
         while (tokens.hasMoreTokens())
         {
            String token = tokens.nextToken();
            if (token.equals("{"))
            {

               token = tokens.nextToken();
               if (Character.isDigit(token.charAt(0)))
               {
                  QueryParameter parameter = new QueryParameter(
                          selectManager,
                          queryMetaData.getMethod(),
                          token);

                  // of if we are here we can assume that we have
                  // a parameter and not a function
                  sqlBuf.append("?");
                  params.add(parameter);


                  if (!tokens.nextToken().equals("}"))
                  {
                     throw new DeploymentException("Invalid parameter - " +
                             "missing closing '}' : " + sql);
                  }
               }
               else
               {
                  // ok we don't have a parameter, we have a function
                  // push the tokens on the buffer and continue
                  sqlBuf.append("{").append(token);
               }
            }
            else
            {
               // not parameter... just append it
               sqlBuf.append(token);
            }
         }
      }

      parameters = params;

      return sqlBuf.toString();
   }

   public static int preloadCmrs(ArrayList deepCmrs, ResultSet rs, int index)
   {
      for (int i = 0; i < deepCmrs.size(); ++i)
      {
         Object[] stuff = (Object[]) deepCmrs.get(i);
         JDBCCMRFieldBridge cmr = (JDBCCMRFieldBridge) stuff[0];
         index = cmr.preloadCmr(rs, index);
      }
      for (int i = 0; i < deepCmrs.size(); ++i)
      {
         Object[] stuff = (Object[]) deepCmrs.get(i);
         ArrayList children = (ArrayList) stuff[1];
         if (children == null) continue;
         index = preloadCmrs(children, rs, index);
      }
      return index;
   }

   public static void generateCmrOuterJoin(ArrayList deepCmrs, String alias, StringBuffer sql)
   {
      for (int i = 0; i < deepCmrs.size(); ++i)
      {
         Object[] stuff = (Object[]) deepCmrs.get(i);
         JDBCCMRFieldBridge cmr = (JDBCCMRFieldBridge) stuff[0];
         JDBCAbstractQueryCommand.generateCmrOuterJoin(cmr, alias, sql);
      }
      for (int i = 0; i < deepCmrs.size(); ++i)
      {
         Object[] stuff = (Object[]) deepCmrs.get(i);
         JDBCCMRFieldBridge cmr = (JDBCCMRFieldBridge) stuff[0];
         ArrayList children = (ArrayList) stuff[1];
         if (children == null) continue;
         generateCmrOuterJoin(children, cmr.getFieldName(), sql);
      }
   }

   public static void cmrColumnNames(ArrayList deepCmrs, StringBuffer columnNamesClause, StringBuffer[] forUpdate)
   {
      for (int i = 0; i < deepCmrs.size(); ++i)
      {
         Object[] stuff = (Object[]) deepCmrs.get(i);
         JDBCCMRFieldBridge cmr = (JDBCCMRFieldBridge) stuff[0];

         columnNamesClause.append(SQLUtil.COMMA);
         StringBuffer cmrClause = new StringBuffer(100);
         SQLUtil.getColumnNamesClause(cmr.getRelatedJDBCEntity().getTableFields(),
                 cmr.getFieldName(), cmrClause);
         if (cmr.getRelatedJDBCEntity().getMetaData().hasRowLocking())
         {
            if (forUpdate[0] == null)
            {
               forUpdate[0] = new StringBuffer(" FOR UPDATE OF ");
            }
            else
            {
               forUpdate[0].append(", ");
            }
            forUpdate[0].append(cmrClause);
         }
         columnNamesClause.append(cmrClause);
      }
      for (int i = 0; i < deepCmrs.size(); ++i)
      {
         Object[] stuff = (Object[]) deepCmrs.get(i);
         ArrayList children = (ArrayList) stuff[1];
         if (children == null) continue;
         cmrColumnNames(children, columnNamesClause, forUpdate);
      }
   }

   public static void generateCmrOuterJoin(JDBCCMRFieldBridge cmr, String alias, StringBuffer leftJoin)
   {
      JDBCEntityBridge childEntity = cmr.getRelatedJDBCEntity();
      leftJoin.append(SQLUtil.LEFT_OUTER_JOIN)
              .append(childEntity.getTableName())
              .append(' ')
              .append(cmr.getFieldName())
              .append(SQLUtil.ON);
      SQLUtil.getJoinClause(cmr, alias, cmr.getFieldName(), leftJoin);
   }

   public static ArrayList deepPreloadableCmrs(JDBCCMRFieldBridge[] cmrs)
   {
      ArrayList cmrlist = new ArrayList(cmrs.length);
      for (int i = 0; i < cmrs.length; i++)
      {
         JDBCStoreManager cmrManager = cmrs[i].getRelatedManager();
         boolean[] cmrLoadMask = cmrs[i].getRelatedJDBCEntity().getEagerLoadMask();
         JDBCCMRFieldBridge[] childCmrs = getPreloadableCmrs(cmrLoadMask, cmrManager);
         ArrayList children = null;
         if (childCmrs != null) children = deepPreloadableCmrs(childCmrs);
         cmrlist.add(new Object[]{cmrs[i], children});
      }
      return cmrlist;
   }

   public static JDBCCMRFieldBridge[] getPreloadableCmrs(boolean[] loadMask, JDBCStoreManager manager)
   {
      JDBCEntityBridge entity = manager.getEntityBridge();
      JDBCCMRFieldBridge[] cmrs = entity.getCMRFields();
      JDBCCMPFieldBridge[] tableFields = entity.getTableFields();
      ArrayList polledCmrs = null;
      if (cmrs != null)
      {
         for (int i = 0; i < cmrs.length; i++)
         {
            if (!cmrs[i].getMetaData().isDeepReadAhead())
               continue;

            if (cmrs[i].getMetaData().getRelatedRole().isMultiplicityOne())
            {
               if (polledCmrs == null) polledCmrs = new ArrayList(cmrs.length);
               polledCmrs.add(cmrs[i]);
            }
            /*
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
                  if (tableFields[l] == f && loadMask[l])
                  {
                     found = true;
                     break;
                  }
               }

               for (int l = 0; l < entity.getPrimaryKeyFields().length && !found; l++)
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
            }
            */
         }
      }
      if (polledCmrs == null) return null;
      return (JDBCCMRFieldBridge[]) polledCmrs.toArray(new JDBCCMRFieldBridge[polledCmrs.size()]);
   }
}
