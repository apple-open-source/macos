/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.jaws.jdbc;

import java.lang.reflect.Method;

import java.sql.PreparedStatement;
import java.util.Iterator;

import org.jboss.ejb.plugins.jaws.metadata.CMPFieldMetaData;
import org.jboss.ejb.plugins.jaws.metadata.FinderMetaData;

import org.jboss.logging.Logger;

/**
 * JAWSPersistenceManager JDBCFindByCommand
 *
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @author <a href="mailto:shevlandj@kpi.com.au">Joe Shevland</a>
 * @author <a href="mailto:justin@j-m-f.demon.co.uk">Justin Forder</a>
 * @author <a href="mailto:danch@nvisia.com">Dan Christopherson</a>
 * @author <a href="mailto:jaeger@oio.de">Torben Jäger</a>
 * @version $Revision: 1.14 $
 */
public class JDBCFindByCommand
   extends JDBCFinderCommand
{
   // Attributes ----------------------------------------------------
   
   // The meta-info for the field we are finding by
   private CMPFieldMetaData cmpField;
   private Logger log = Logger.getLogger(JDBCFindByCommand.class);

   // Constructors --------------------------------------------------
   
   public JDBCFindByCommand(JDBCCommandFactory factory,
                            Method finderMethod,
                            FinderMetaData md)
      throws IllegalArgumentException
   {
      super(factory, md);
      
      String cmpFieldName = finderMethod.getName().substring(6).toLowerCase();

      if (log.isDebugEnabled()) {
         log.debug("cmp field name: " + cmpFieldName);
      }
      
      // Find the meta-info for the field we want to find by
      
      cmpField = null;
      Iterator iter = jawsEntity.getCMPFields();
      
      while (cmpField == null && iter.hasNext())
      {
         CMPFieldMetaData fi = (CMPFieldMetaData)iter.next();
         
         String lastComponentOfName = 
            CMPFieldMetaData.getLastComponent(fi.getName()).toLowerCase();
         if (cmpFieldName.equals(lastComponentOfName))
         {
            cmpField = fi;
         }
      }
      
      if (cmpField == null)
      {
         throw new IllegalArgumentException(
            "No finder for this method: " + finderMethod.getName());
      }
      
      // Compute SQL
      
      String sql = "SELECT " + getPkColumnList() +
                   " FROM "+jawsEntity.getTableName()+ " WHERE ";
      
      sql += cmpField.getColumnName() + "=?";
      
      setSQL(sql);
   }
   
   // JDBCQueryCommand overrides

   public String getWhereClause() {
      return cmpField.getColumnName() + "=?";
   }

   public String getFromClause() {
      return " FROM "+jawsEntity.getTableName();
   }
   
   public String getOrderByClause() {
      return "";
   }
   
   // JDBCFinderCommand overrides -----------------------------------
   
   protected void setParameters(PreparedStatement stmt, Object argOrArgs) 
      throws Exception
   {
      Object[] args = (Object[])argOrArgs;
      
      if (cmpField != null)
      {
         setParameter(stmt, 1, cmpField.getJDBCType(), args[0]);
      }
   }
}
