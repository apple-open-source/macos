/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc.bridge;


import org.jboss.deployment.DeploymentException;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCStoreManager;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCCMPFieldMetaData;
import org.jboss.ejb.EntityEnterpriseContext;

import java.sql.PreparedStatement;

/**
 * The base class for all automatically updated fields such as audit and version.
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 * @version $Revision: 1.1.2.1 $
 */
public abstract class JDBCCMP2xAutoUpdatedFieldBridge extends JDBCCMP2xFieldBridge
{
   // Constructors

   public JDBCCMP2xAutoUpdatedFieldBridge(JDBCStoreManager manager,
                                          JDBCCMPFieldMetaData metadata)
      throws DeploymentException
   {
      super(manager, metadata);
      defaultFlags |= JDBCEntityBridge.ADD_TO_SET_ON_UPDATE;
   }

   public JDBCCMP2xAutoUpdatedFieldBridge(JDBCCMP2xFieldBridge cmpField)
      throws DeploymentException
   {
      super(
         cmpField.getManager(),
         cmpField.getFieldName(),
         cmpField.getFieldType(),
         cmpField.getJDBCType(),
         cmpField.isReadOnly(),               // should always be false?
         cmpField.getReadTimeOut(),
         cmpField.getPrimaryKeyClass(),
         cmpField.getPrimaryKeyField(),
         cmpField,
         null,                                // it should not be a foreign key
         cmpField.getColumnName()
      );
      defaultFlags |= JDBCEntityBridge.ADD_TO_SET_ON_UPDATE; // it should be redundant
      cmpField.addDefaultFlag(JDBCEntityBridge.ADD_TO_SET_ON_UPDATE);
   }

   public void initInstance(EntityEnterpriseContext ctx)
   {
      setFirstVersion(ctx);
   }

   public int setInstanceParameters(PreparedStatement ps,
                                    int parameterIndex,
                                    EntityEnterpriseContext ctx)
   {
      Object value;
      if(ctx.isValid())
      {
         // update
         // generate new value unless it is already provided by the user
         value = isDirty(ctx) ? getInstanceValue(ctx) : updateVersion(ctx);
      }
      else
      {
         // create
         value = getInstanceValue(ctx);
      }
      return setArgumentParameters(ps, parameterIndex, value);
   }

   public abstract void setFirstVersion(EntityEnterpriseContext ctx);
   public abstract Object updateVersion(EntityEnterpriseContext ctx);
}
