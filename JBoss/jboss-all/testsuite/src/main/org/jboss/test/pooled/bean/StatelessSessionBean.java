
//Title:        Yo MAMA!
//Version:
//Copyright:    Copyright (c) 1999
//Author:       Bill Burke
//Description:  Your description

package org.jboss.test.pooled.bean;

import java.rmi.*;
import javax.ejb.*;
import javax.naming.InitialContext;
import javax.naming.Context;

public class StatelessSessionBean implements SessionBean 
{
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
   
  private SessionContext sessionContext;

   public void ejbCreate() throws RemoteException, CreateException 
   {
   }
   
   public void ejbActivate() throws RemoteException {
   }
   
   public void ejbPassivate() throws RemoteException {
   }
   
   public void ejbRemove() throws RemoteException {
   }
   
   public void setSessionContext(SessionContext context) throws RemoteException {
      sessionContext = context;
      //Exception e = new Exception("in set Session context");
      //log.debug("failed", e);
   }
   
   public void noop() throws RemoteException
   {
      return;
   }
}
