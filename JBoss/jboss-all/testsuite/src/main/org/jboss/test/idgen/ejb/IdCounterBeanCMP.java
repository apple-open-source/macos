/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.idgen.ejb;

import java.io.ObjectStreamException;
import java.rmi.RemoteException;
import javax.ejb.CreateException;

/**
 *      
 *   @see <related>
 *   @author $Author: user57 $
 *   @version $Revision: 1.2 $
 */
public class IdCounterBeanCMP
   extends IdCounterBean
{
   public String name;
   public long currentValue;
   
   public long getCurrentValue()
   {
        return currentValue;
   }
	
   public void setCurrentValue(long current)
   {
   	this.currentValue = current;
   }
   
   public String getName()
   {
   	return name;
   }
	
   public void setName(String beanName)
   {
   	this.name = beanName;
   }
   
   public String ejbCreate(String name) 
      throws RemoteException, CreateException
   { 
      setName(name);
      currentValue = 0;
		
      return null;
   }
   
   public void ejbPostCreate(String name) 
      throws RemoteException, CreateException
   { 
   }
}
