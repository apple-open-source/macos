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
 * CMPStoreManager JDBCActivateEntityCommand
 *    
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.6.4.4 $
 */
public final class JDBCInitEntityCommand {
   private final JDBCEntityBridge entity;
   
   public JDBCInitEntityCommand(JDBCStoreManager manager) {
      entity = manager.getEntityBridge();
   }
   
   /**
    * Called before ejbCreate. In the JDBCStoreManager we need to 
    * initialize the presistence context. The persistence context is where
    * where bean data is stored. If CMP 1.x, original values are store 
    * and for CMP 2.x actual values are stored int the context. Then we
    * initialize the data. In CMP 1.x fields are reset to Java defaults, and
    * in CMP 2.x current value in persistence store are initialized.
    *
    * Note: persistence context is also initialized in activate.
    */
   public void execute(EntityEnterpriseContext ctx) {
      entity.initPersistenceContext(ctx);
      entity.initInstance(ctx);
   }
}
