/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.cmp.jdbc;

import org.jboss.deployment.DeploymentException;
import org.jboss.ejb.plugins.cmp.ejbql.Catalog;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCEntityBridge;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCJBossQLQueryMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCQueryMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCReadAheadMetaData;

/**
 * This class generates a query from JBoss-QL.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.3.2.2 $
 */
public class JDBCJBossQLQuery extends JDBCAbstractQueryCommand {

   public JDBCJBossQLQuery(
         JDBCStoreManager manager,
         JDBCQueryMetaData q) throws DeploymentException {

      super(manager, q);

      JDBCJBossQLQueryMetaData metadata = (JDBCJBossQLQueryMetaData)q;
      if(getLog().isDebugEnabled()) {
         getLog().debug("JBossQL: " + metadata.getJBossQL());
      }

      JDBCEJBQLCompiler compiler = new JDBCEJBQLCompiler(
            (Catalog)manager.getApplicationData("CATALOG"));

      try {
         compiler.compileJBossQL(
               metadata.getJBossQL(),
               metadata.getMethod().getReturnType(),
               metadata.getMethod().getParameterTypes(),
               metadata.getReadAhead());
      } catch(Throwable t) {
         throw new DeploymentException("Error compiling JBossQL " +
            "statement '" + metadata.getJBossQL() + "'", t);
      }

      setSQL(compiler.getSQL());
      setOffsetParam(compiler.getOffset());
      setLimitParam(compiler.getLimit());

      // set select object
      if(compiler.isSelectEntity()) {
         JDBCEntityBridge selectEntity = compiler.getSelectEntity();

         // set the select entity
         setSelectEntity(selectEntity);

         // set the preload fields
         JDBCReadAheadMetaData readahead = metadata.getReadAhead();
         if(readahead.isOnFind()) {
            String eagerLoadGroup = readahead.getEagerLoadGroup();
            setPreloadFields(selectEntity.getLoadGroup(eagerLoadGroup));
         }
      } else {
         setSelectField(compiler.getSelectField());
      }

      // get the parameter order
      setParameterList(compiler.getInputParameters());
   }
}
