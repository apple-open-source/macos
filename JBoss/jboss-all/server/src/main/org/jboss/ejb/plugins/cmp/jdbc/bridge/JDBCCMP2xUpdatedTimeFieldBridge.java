/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc.bridge;

import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCStoreManager;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCTypeFactory;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCCMPFieldMetaData;
import org.jboss.deployment.DeploymentException;

/**
 * Audit updated time field.
 *
 * @author <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
 */
public class JDBCCMP2xUpdatedTimeFieldBridge extends JDBCCMP2xAutoUpdatedFieldBridge
{
   public JDBCCMP2xUpdatedTimeFieldBridge(JDBCStoreManager manager,
                                               JDBCCMPFieldMetaData metadata)
      throws DeploymentException
   {
      super(manager, metadata);
      checkDirtyAfterGet = false;
      stateFactory = JDBCTypeFactory.EQUALS;
   }

   public JDBCCMP2xUpdatedTimeFieldBridge(JDBCCMP2xFieldBridge cmpField)
      throws DeploymentException
   {
      super(cmpField);
      checkDirtyAfterGet = false;
      stateFactory = JDBCTypeFactory.EQUALS;
   }

   public void setFirstVersion(EntityEnterpriseContext ctx)
   {
      setInstanceValue(ctx, new java.util.Date());
   }

   public Object updateVersion(EntityEnterpriseContext ctx)
   {
      Object value = new java.util.Date();
      setInstanceValue(ctx, value);
      return value;
   }
}
