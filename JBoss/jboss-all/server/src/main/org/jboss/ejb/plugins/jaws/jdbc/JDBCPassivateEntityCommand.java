/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.jaws.jdbc;

import java.rmi.RemoteException;

import org.jboss.ejb.EntityEnterpriseContext;

import org.jboss.ejb.plugins.jaws.JPMPassivateEntityCommand;

/**
 * JAWSPersistenceManager JDBCPassivateEntityCommand
 *    
 * @see <related>
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:justin@j-m-f.demon.co.uk">Justin Forder</a>
 * @version $Revision: 1.3 $
 */
 
public class JDBCPassivateEntityCommand implements JPMPassivateEntityCommand
{
   // Constructors --------------------------------------------------
   
   public JDBCPassivateEntityCommand(JDBCCommandFactory factory)
   {
   }
   
   // JPMPassivateEntityCommand implementation ----------------------
   
   public void execute(EntityEnterpriseContext ctx) throws RemoteException
   {
      // There is nothing to do here.
   }
}
