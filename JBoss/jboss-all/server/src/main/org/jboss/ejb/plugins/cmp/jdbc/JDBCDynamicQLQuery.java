/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.cmp.jdbc;

import java.lang.reflect.Method;
import java.util.Collection;
import javax.ejb.FinderException;
import org.jboss.deployment.DeploymentException;
import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.ejb.plugins.cmp.ejbql.Catalog;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCEntityBridge;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCQueryMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCDynamicQLQueryMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCReadAheadMetaData;

/**
 * This class generates a query from JBoss-QL.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.2.2.2 $
 */
public class JDBCDynamicQLQuery extends JDBCAbstractQueryCommand {
   private final Catalog catalog;
   private JDBCDynamicQLQueryMetaData metadata;

   public JDBCDynamicQLQuery(
         JDBCStoreManager manager, 
         JDBCQueryMetaData q) throws DeploymentException {

      super(manager, q);
      catalog = (Catalog)manager.getApplicationData("CATALOG");
      metadata = (JDBCDynamicQLQueryMetaData)q;
   }

   public Collection execute(
         Method finderMethod,
         Object[] args,
         EntityEnterpriseContext ctx) throws FinderException {

      String dynamicQL = (String)args[0];
      if(getLog().isDebugEnabled()) {
         getLog().debug("DYNAMIC-QL: " + dynamicQL);
      }
      
      JDBCEJBQLCompiler compiler = new JDBCEJBQLCompiler(catalog);

      // get the parameters
      Object[] parameters = (Object[])args[1];
      if(parameters == null) {
         throw new FinderException("Parameters is null");
      }

      // get the parameter types
      Class[] parameterTypes = new Class[parameters.length];
      for(int i=0; i<parameters.length; i++) {
         if(parameters[i] == null) {
            throw new FinderException("Parameter["+i+"] is null");
         }
         parameterTypes[i] = parameters[i].getClass();
      }

      // compile the dynamic-ql
      try {
         compiler.compileJBossQL(
               dynamicQL,
               finderMethod.getReturnType(),
               parameterTypes,
               metadata.getReadAhead());
      } catch(Throwable t) {
         throw new FinderException("Error compiling ejbql: " + t);
      }
      
      // set the sql
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

      return super.execute(finderMethod, parameters, ctx);
   }
}
