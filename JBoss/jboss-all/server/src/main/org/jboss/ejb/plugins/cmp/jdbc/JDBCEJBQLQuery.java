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
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCQlQueryMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCQueryMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCReadAheadMetaData;

/**
 * This class generates a query from EJB-QL.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.8.2.1 $
 */
public class JDBCEJBQLQuery extends JDBCAbstractQueryCommand {

   public JDBCEJBQLQuery(
         JDBCStoreManager manager, 
         JDBCQueryMetaData q) throws DeploymentException {

      super(manager, q);

      JDBCQlQueryMetaData metadata = (JDBCQlQueryMetaData)q;
      if(getLog().isDebugEnabled()) {
         getLog().debug("EJB-QL: " + metadata.getEjbQl());
      }

      JDBCEJBQLCompiler compiler = new JDBCEJBQLCompiler(
            (Catalog)manager.getApplicationData("CATALOG"));

      try {
         compiler.compileEJBQL(
               metadata.getEjbQl(),
               metadata.getMethod().getReturnType(),
               metadata.getMethod().getParameterTypes(),
               metadata.getReadAhead());
      } catch(Throwable t) {
         throw new DeploymentException("Error compiling EJB-QL " +
            "statement '" + metadata.getEjbQl() + "'", t);
      }

      setSQL(compiler.getSQL());

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
