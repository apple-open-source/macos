/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.cmp.jdbc;

import org.jboss.deployment.DeploymentException;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCEntityBridge;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCJBossQLQueryMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCQueryMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCReadAheadMetaData;

/**
 * This class generates a query from JBoss-QL.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 * @version $Revision: 1.3.2.10 $
 */
public final class JDBCJBossQLQuery extends JDBCAbstractQueryCommand
{

   public JDBCJBossQLQuery(JDBCStoreManager manager,
                           JDBCQueryMetaData q)
      throws DeploymentException
   {
      super(manager, q);

      JDBCJBossQLQueryMetaData metadata = (JDBCJBossQLQueryMetaData) q;
      if(getLog().isDebugEnabled())
      {
         getLog().debug("JBossQL: " + metadata.getJBossQL());
      }

      JDBCEJBQLCompiler compiler = new JDBCEJBQLCompiler(manager.getCatalog());

      try
      {
         compiler.compileJBossQL(
            metadata.getJBossQL(),
            metadata.getMethod().getReturnType(),
            metadata.getMethod().getParameterTypes(),
            metadata.getReadAhead());
      }
      catch(Throwable t)
      {
         t.printStackTrace();
         throw new DeploymentException("Error compiling JBossQL " +
            "statement '" + metadata.getJBossQL() + "'", t);
      }

      setSQL(compiler.getSQL());
      setOffsetParam(compiler.getOffsetParam());
      setOffsetValue(compiler.getOffsetValue());
      setLimitParam(compiler.getLimitParam());
      setLimitValue(compiler.getLimitValue());

      // set select object
      if(compiler.isSelectEntity())
      {
         JDBCEntityBridge selectEntity = compiler.getSelectEntity();

         // set the select entity
         setSelectEntity(selectEntity);

         // set the preload fields
         JDBCReadAheadMetaData readahead = metadata.getReadAhead();
         if(readahead.isOnFind())
         {
            setEagerLoadGroup(readahead.getEagerLoadGroup());
            preloadableCmrs = getPreloadableCmrs(getEagerLoadMask(), selectEntity.getManager());
            deepCmrs = null;
            if (preloadableCmrs != null && preloadableCmrs.length > 0)
            {
               deepCmrs = JDBCAbstractQueryCommand.deepPreloadableCmrs(preloadableCmrs);
            }
         }
      }
      else
         if(compiler.isSelectField())
         {
            setSelectField(compiler.getSelectField());
         }
         else
         {
            setSelectFunction(compiler.getSelectFunction(), compiler.getStoreManager());
         }

      // get the parameter order
      setParameterList(compiler.getInputParameters());
   }
}
