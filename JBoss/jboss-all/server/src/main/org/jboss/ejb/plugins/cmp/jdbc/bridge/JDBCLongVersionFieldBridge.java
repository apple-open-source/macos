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
 *
 * @author <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
 */
public class JDBCLongVersionFieldBridge extends JDBCCMP2xVersionFieldBridge
{
   private static final Long FIRST_VERSION = new Long(1);

   public JDBCLongVersionFieldBridge(JDBCStoreManager manager,
                                     JDBCCMPFieldMetaData metadata)
      throws DeploymentException
   {
      super(manager, metadata);
   }

   public JDBCLongVersionFieldBridge(JDBCCMP2xFieldBridge cmpField)
      throws DeploymentException
   {
      super(cmpField);
   }

   public void setFirstVersion(EntityEnterpriseContext ctx)
   {
      setInstanceValue(ctx, FIRST_VERSION);
   }

   public Object updateVersion(EntityEnterpriseContext ctx)
   {
      long current = ((Long)getInstanceValue(ctx)).longValue();
      Long next = new Long(current + 1);
      setInstanceValue(ctx, next);
      return next;
   }
}
