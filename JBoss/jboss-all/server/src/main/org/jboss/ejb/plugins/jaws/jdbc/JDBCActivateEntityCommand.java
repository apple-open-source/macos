/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.jaws.jdbc;

import java.rmi.RemoteException;


import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.ejb.plugins.jaws.JAWSPersistenceManager;
import org.jboss.ejb.plugins.jaws.JPMActivateEntityCommand;

/**
 * JAWSPersistenceManager JDBCActivateEntityCommand
 *    
 * @see <related>
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:justin@j-m-f.demon.co.uk">Justin Forder</a>
 * @version $Revision: 1.4 $
 */
 
public class JDBCActivateEntityCommand implements JPMActivateEntityCommand
{
   // Constructors --------------------------------------------------
   
   public JDBCActivateEntityCommand(JDBCCommandFactory factory)
   {
   }
   
   // JPMActivateEntityCommand implementation -----------------------
   
   public void execute(EntityEnterpriseContext ctx)
      throws RemoteException
   {
      // Set new persistence context
      // JF: Passivation/Activation is losing persistence context!!!
      ctx.setPersistenceContext(new JAWSPersistenceManager.PersistenceContext());
   }
}
