/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.jdbc;

import java.lang.reflect.Method;
import org.jboss.ejb.EntityEnterpriseContext;

/**
 * JDBCIsModifiedCommand determines if the entity has been modified.
 *    
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.1 $
 */
 
public class JDBCIsModifiedCommand {
   public JDBCIsModifiedCommand(JDBCStoreManager manager) {
   }
   
   public boolean execute(EntityEnterpriseContext ctx) {
     return true;
   }
}

