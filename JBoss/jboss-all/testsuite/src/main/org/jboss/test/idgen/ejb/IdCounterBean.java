/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.idgen.ejb;

import java.rmi.RemoteException;
import javax.ejb.*;
import javax.naming.*;

import org.jboss.test.util.ejb.EntitySupport;

/**
 *      
 *   @see <related>
 *   @author $Author: user57 $
 *   @version $Revision: 1.2 $
 */
public abstract class IdCounterBean
   extends EntitySupport
{
   long nextId;
   long size;
   
   public long getNextValue()
   {
      // Is sequence finished?
      // If so start a new one

      if (nextId == (getCurrentValue() + size))
      {
         setCurrentValue(nextId);
      }
      
      return nextId++;
   }
   
   public abstract long getCurrentValue();
   public abstract void setCurrentValue(long current);
	
   public abstract String getName();
   public abstract void setName(String beanName);
	
   public void ejbLoad()
      throws RemoteException
   {
      nextId = getCurrentValue();
   }
	
   public void setEntityContext(EntityContext ctx)
      throws RemoteException
   {
      super.setEntityContext(ctx);
      
      try {
         size = ((Long)new InitialContext().lookup("java:comp/env/size")).longValue();
      } 
      catch (Exception e) {
         throw new EJBException(e);
      }
   }
}
