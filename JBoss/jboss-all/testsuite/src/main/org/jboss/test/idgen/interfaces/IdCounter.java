/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.idgen.interfaces;

import java.rmi.*;
import javax.ejb.*;

/**
 *      
 *   @see <related>
 *   @author $Author: user57 $
 *   @version $Revision: 1.2 $
 */
public interface IdCounter
   extends EJBObject
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
   
   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   
   // Public --------------------------------------------------------
   public long getNextValue()
      throws RemoteException;
		
   public String getName()
      throws RemoteException;
}
