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
 * JDBCPassivateEntityCommand deletes the entity persistence context,
 * where data about the instence is keeps.
 *    
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:justin@j-m-f.demon.co.uk">Justin Forder</a>
 * @version $Revision: 1.6 $
 */
 
public class JDBCPassivateEntityCommand {
   private JDBCEntityBridge entity;

   public JDBCPassivateEntityCommand(JDBCStoreManager manager) {
      entity = manager.getEntityBridge();
   }
   
   public void execute(EntityEnterpriseContext ctx) {
      entity.destroyPersistenceContext(ctx);
   }
}
