/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.cmp.jdbc;

import java.util.ArrayList;
import java.util.List;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCEntityBridge;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCQueryMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCReadAheadMetaData;

/**
 * JDBCFindAllQuery automatic finder used in CMP 1.x.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @author <a href="mailto:shevlandj@kpi.com.au">Joe Shevland</a>
 * @author <a href="mailto:justin@j-m-f.demon.co.uk">Justin Forder</a>
 * @version $Revision: 1.3 $
 */
public class JDBCFindAllQuery extends JDBCAbstractQueryCommand {
   
   public JDBCFindAllQuery(JDBCStoreManager manager, JDBCQueryMetaData q) {
      super(manager, q);

      JDBCEntityBridge entity = manager.getEntityBridge();

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
      
      setSQL(sql.toString());
   }
}
