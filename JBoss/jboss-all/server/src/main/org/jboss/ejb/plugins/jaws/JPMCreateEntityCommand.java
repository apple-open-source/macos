/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.jaws;

import java.lang.reflect.Method;
import org.jboss.ejb.EntityEnterpriseContext;
import java.rmi.RemoteException;
import javax.ejb.CreateException;

/**
 * Interface for JAWSPersistenceManager CreateEntity Command.
 *      
 * @author <a href="mailto:justin@j-m-f.demon.co.uk">Justin Forder</a>
 * @version $Revision: 1.3 $
 */
public interface JPMCreateEntityCommand
{
   // Public --------------------------------------------------------
   
   public Object execute(Method m, 
                         Object[] args, 
                         EntityEnterpriseContext ctx)
      throws RemoteException, CreateException;
}
