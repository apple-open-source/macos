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

/**
 * The base class for optimistic locking version fields (sequence and timestamp).
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 * @version $Revision: 1.1.2.3 $
 */
public abstract class JDBCCMP2xVersionFieldBridge extends JDBCCMP2xAutoUpdatedFieldBridge
{
   // Constructors

   public JDBCCMP2xVersionFieldBridge(JDBCStoreManager manager,
                                      JDBCCMPFieldMetaData metadata)
      throws DeploymentException
   {
      super(manager, metadata);
      defaultFlags |= JDBCEntityBridge.ADD_TO_WHERE_ON_UPDATE;
   }

   public JDBCCMP2xVersionFieldBridge(JDBCCMP2xFieldBridge cmpField)
      throws DeploymentException
   {
      super(cmpField);
      defaultFlags |= JDBCEntityBridge.ADD_TO_WHERE_ON_UPDATE;
      cmpField.addDefaultFlag(JDBCEntityBridge.ADD_TO_WHERE_ON_UPDATE);
   }
}
