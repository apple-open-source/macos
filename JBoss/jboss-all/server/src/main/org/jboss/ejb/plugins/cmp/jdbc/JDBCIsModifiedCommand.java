/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.jdbc;

import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCEntityBridge;


/**
 * JDBCIsModifiedCommand determines if the entity has been modified.
 *    
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
 * @version $Revision: 1.1.4.2 $
 */
 
public final class JDBCIsModifiedCommand
{
   private final JDBCEntityBridge bridge;

   public JDBCIsModifiedCommand(JDBCStoreManager manager)
   {
      bridge = manager.getEntityBridge();
   }
   
   public boolean execute(EntityEnterpriseContext ctx)
   {
      return bridge.isCreated(ctx);
   }
}

