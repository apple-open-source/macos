/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.idgen.ejb;

import java.rmi.*;
import javax.naming.*;
import javax.ejb.*;

import org.jboss.test.util.ejb.SessionSupport;
import org.jboss.test.idgen.interfaces.*;

/**
 *      
 *   @see <related>
 *   @author $Author: user57 $
 *   @version $Revision: 1.3 $
 */
public class IdGeneratorBean
   extends SessionSupport
{
   IdCounterHome counterHome;
   
   static final String SIZE = "java:comp/env/size";

   public long getNewId(String beanName)
      throws RemoteException
   {
      IdCounter counter;
      
      // Acquire counter
      try {
         counter = counterHome.findByPrimaryKey(beanName);
      } 
      catch (FinderException e) {
         try {
            counter = counterHome.create(beanName);
         } catch (CreateException ex) {
            throw new EJBException("Could not find or create counter for "+beanName);
         }
      }
      
      // Get id
      return counter.getNextValue();
   }

   public void setSessionContext(SessionContext context) 
   {
      super.setSessionContext(context);
      
      try {
         counterHome = (IdCounterHome)new InitialContext().lookup("java:comp/env/ejb/IdCounter");
      } 
      catch (Exception e) {
         throw new EJBException(e);
      }
   }
}
