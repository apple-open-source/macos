/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc.bridge;

import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCStoreManager;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCCMPFieldMetaData;
import org.jboss.deployment.DeploymentException;

/**
 * Audit updated principal field.
 *
 * @author <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
 */
public class JDBCCMP2xUpdatedPrincipalFieldBridge extends JDBCCMP2xAutoUpdatedFieldBridge
{
   public JDBCCMP2xUpdatedPrincipalFieldBridge(JDBCStoreManager manager,
                                               JDBCCMPFieldMetaData metadata)
      throws DeploymentException
   {
      super(manager, metadata);
   }

   public JDBCCMP2xUpdatedPrincipalFieldBridge(JDBCCMP2xFieldBridge cmpField)
      throws DeploymentException
   {
      super(cmpField);
   }

   public void setFirstVersion(EntityEnterpriseContext ctx)
   {
      setInstanceValue(ctx, ctx.getEJBContext().getCallerPrincipal().getName());
   }

   public Object updateVersion(EntityEnterpriseContext ctx)
   {
      Object value = ctx.getEJBContext().getCallerPrincipal().getName();
      setInstanceValue(ctx, value);
      return value;
   }
}
