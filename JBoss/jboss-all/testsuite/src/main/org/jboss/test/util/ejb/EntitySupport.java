/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.util.ejb;

import java.rmi.RemoteException;
import javax.ejb.*;

/**
 *      
 *   @see <related>
 *   @author $Author: oberg $
 *   @version $Revision: 1.1.1.1 $
 */
public abstract class EntitySupport
   extends EnterpriseSupport
   implements EntityBean
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
   protected EntityContext entityCtx;
   
   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   
   // Public --------------------------------------------------------
   public void setEntityContext(EntityContext ctx) 
      throws RemoteException
   {
      entityCtx = ctx;
   }
   public void unsetEntityContext() 
      throws RemoteException
   { 
   }
	
   public void ejbActivate() 
      throws RemoteException
   {
   }
	
   public void ejbPassivate() 
      throws RemoteException
   {
   }
	
   public void ejbLoad()
      throws RemoteException
   {
   }
	
   public void ejbStore() 
      throws RemoteException
   { 
   }
	
   public void ejbRemove() 
      throws RemoteException, RemoveException
   {
   }
}
