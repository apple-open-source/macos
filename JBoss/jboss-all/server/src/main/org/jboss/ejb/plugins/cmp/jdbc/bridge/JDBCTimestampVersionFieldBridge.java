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
 *
 * @author <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
 */
public class JDBCTimestampVersionFieldBridge extends JDBCCMP2xVersionFieldBridge
{
   public JDBCTimestampVersionFieldBridge(JDBCStoreManager manager,
                                          JDBCCMPFieldMetaData metadata)
      throws DeploymentException
   {
      super(manager, metadata);
      checkDirtyAfterGet = false;
      stateFactory = JDBCTypeFactory.EQUALS;
   }

   public JDBCTimestampVersionFieldBridge(JDBCCMP2xFieldBridge cmpField)
      throws DeploymentException
   {
      super(cmpField);
      checkDirtyAfterGet = false;
      stateFactory = JDBCTypeFactory.EQUALS;
   }

   public void setFirstVersion(EntityEnterpriseContext ctx)
   {
      Object version = new java.util.Date();
      setInstanceValue(ctx, version);
   }

   public Object updateVersion(EntityEnterpriseContext ctx)
   {
      Object next = new java.util.Date();
      setInstanceValue(ctx, next);
      return next;
   }
}
