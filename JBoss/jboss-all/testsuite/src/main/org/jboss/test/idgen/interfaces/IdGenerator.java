/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.idgen.interfaces;

import java.rmi.*;
import javax.ejb.*;
import javax.naming.*;

/**
 *      
 *   @see <related>
 *   @author $Author: oberg $
 *   @version $Revision: 1.1 $
 */
public interface IdGenerator
   extends EJBObject
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
   
   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   
   // Public --------------------------------------------------------
   public long getNewId(String beanName)
      throws RemoteException;
   
}
