/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.jaws.jdbc;

import org.jboss.ejb.plugins.jaws.JPMStopCommand;

/**
 * JAWSPersistenceManager JDBCStopCommand
 *    
 * @see <related>
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:justin@j-m-f.demon.co.uk">Justin Forder</a>
 * @version $Revision: 1.3 $
 */
public class JDBCStopCommand implements JPMStopCommand
{
   // Constructors --------------------------------------------------
   
   public JDBCStopCommand(JDBCCommandFactory factory)
   {
   }
   
   // JPMStopCommand implementation ---------------------------------
   
   public void execute()
   {
   }
}
