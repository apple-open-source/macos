/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.util.ejb;

import javax.ejb.*;

import org.apache.log4j.Category;

/**
 * 
 *   @see <related>
 *   @version $Revision: 1.3 $
 */
public abstract class SessionSupport
   extends EnterpriseSupport
   implements SessionBean
{
   protected transient Category log = Category.getInstance(getClass());

   protected SessionContext sessionCtx;
   
   public void ejbCreate()
      throws CreateException
   {
   }
   
   public void setSessionContext(SessionContext ctx) 
   {
      sessionCtx = ctx;
   }
	
   public void ejbActivate() 
   {
   }
	
   public void ejbPassivate() 
   {
   }
	
   public void ejbRemove() 
   {
   }

   private void writeObject(java.io.ObjectOutputStream stream)
      throws java.io.IOException
   {
      // nothing
   }
   
   private void readObject(java.io.ObjectInputStream stream)
      throws java.io.IOException, ClassNotFoundException
   {
      // reset logging
      log = Category.getInstance(getClass());
   }
}
