/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc;

import java.util.ArrayList;
import java.util.List;

import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMPFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCEntityBridge; 
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCQueryMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCReadAheadMetaData;

/**
 * JDBCFindByQuery automatic finder used in CMP 1.x.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @author <a href="mailto:shevlandj@kpi.com.au">Joe Shevland</a>
 * @author <a href="mailto:justin@j-m-f.demon.co.uk">Justin Forder</a>
 * @author <a href="mailto:danch@nvisia.com">danch (Dan Christopherson)</a>
 * @version $Revision: 1.3 $
 */
public class JDBCFindByQuery extends JDBCAbstractQueryCommand {
   
   // The meta-info for the field we are finding by
   private JDBCCMPFieldBridge cmpField;
   
   public JDBCFindByQuery(JDBCStoreManager manager, JDBCQueryMetaData q)
      throws IllegalArgumentException {

      super(manager, q);
      
      JDBCEntityBridge entity = manager.getEntityBridge();

      String finderName = q.getMethod().getName();
      
      // finder name will be like findByFieldName
      // we need to convert it to fieldName.
      String cmpFieldName = Character.toLowerCase(finderName.charAt(6)) +
            finderName.substring(7);

      // get the field
      cmpField = (JDBCCMPFieldBridge)entity.getCMPFieldByName(cmpFieldName);
      if(cmpField == null) {
         throw new IllegalArgumentException(
               "No finder for this method: " + finderName);
      }
      
      // set the preload fields
      JDBCReadAheadMetaData readAhead = q.getReadAhead();
      if(readAhead.isOnFind()) {
         String eagerLoadGroupName = readAhead.getEagerLoadGroup();
         setPreloadFields(entity.getLoadGroup(eagerLoadGroupName));
      }

      // get a list of all fields to be loaded
      List loadFields = new ArrayList();
      loadFields.addAll(entity.getPrimaryKeyFields());
      loadFields.addAll(getPreloadFields());

      // generate the sql
      StringBuffer sql = new StringBuffer();
      sql.append("SELECT ").append(SQLUtil.getColumnNamesClause(loadFields));
      sql.append(" FROM ").append(entity.getTableName());
      sql.append(" WHERE ").append(SQLUtil.getWhereClause(cmpField));
      
      setSQL(sql.toString());
      setParameterList(QueryParameter.createParameters(0, cmpField));
   }
}
