
//Title:        Yo MAMA!
//Version:
//Copyright:    Copyright (c) 1999
//Author:       Bill Burke
//Description:  Your description

package org.jboss.test.deadlock.bean;

import java.rmi.*;
import javax.ejb.*;
import javax.naming.InitialContext;
import javax.naming.Context;
import org.jboss.test.deadlock.interfaces.*;
import org.jboss.util.deadlock.ApplicationDeadlockException;

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
   
   
   public void callAB() throws RemoteException
   {
      try
      {
	 log.info("****callAB start****");
	 EnterpriseEntityHome home = (EnterpriseEntityHome)new InitialContext().lookup("nextgenEnterpriseEntity");
	 EnterpriseEntity A = home.findByPrimaryKey("A");
	 EnterpriseEntity B = home.findByPrimaryKey("B");
	 A.getOtherField();
	 log.debug("callAB is sleeping");
	 Thread.sleep(1000);
	 log.debug("callAB woke up");
	 B.getOtherField();
	 log.debug("callAB end");
      }
      catch (ApplicationDeadlockException ade)
      {
         System.out.println("APPLICATION DEADLOCK EXCEPTION");
         throw ade;
      }
      catch (RemoteException rex)
      {
         throw rex;
      }
      catch (Exception ex)
      {
	 throw new RemoteException("failed");
      }
   }
   
   public void callBA() throws RemoteException
   {
      try
      {
	 log.info("****callBA start****");
	 EnterpriseEntityHome home = (EnterpriseEntityHome)new InitialContext().lookup("nextgenEnterpriseEntity");
	 EnterpriseEntity B = home.findByPrimaryKey("B");
	 EnterpriseEntity A = home.findByPrimaryKey("A");
	 B.getOtherField();
	 log.debug("callBA is sleeping");
	 Thread.sleep(1000);
	 log.debug("callBA woke up");
	 A.getOtherField();
	 log.debug("callBA end");
      }
      catch (ApplicationDeadlockException ade)
      {
         System.out.println("APPLICATION DEADLOCK EXCEPTION");
         throw ade;
      }
      catch (RemoteException rex)
      {
         throw rex;
      }
      catch (Exception ex)
      {
	 throw new RemoteException("failed");
      }
   }

   public void requiresNewTest(boolean first) throws RemoteException
   {
      try
      {
	 log.info("***requiresNewTest start***");
         InitialContext ctx = new InitialContext();
	 EnterpriseEntityHome home = (EnterpriseEntityHome)ctx.lookup("nextgenEnterpriseEntity");
	 EnterpriseEntity C = home.findByPrimaryKey("C");

         C.getOtherField();
         if (first)
         {
            StatelessSessionHome shome = (StatelessSessionHome)ctx.lookup("nextgen.StatelessSession");
            StatelessSession session = shome.create();
            session.requiresNewTest(false);
         }
      }
      catch (RemoteException rex)
      {
         throw rex;
      }
      catch (Exception ex)
      {
         throw new RemoteException("failed");
      }
   }

   public void createCMRTestData(String jndiName)
   {
      try
      {
         InitialContext ctx = new InitialContext();
         EnterpriseEntityLocalHome home = (EnterpriseEntityLocalHome)ctx.lookup(jndiName);
         try
         {
            home.create("First");
         }
         catch (DuplicateKeyException dontCare)
         {
         }
         try
         {
            home.create("Second");
         }
         catch (DuplicateKeyException dontCare)
         {
         }
         EnterpriseEntityLocal first = home.findByPrimaryKey("First");
         EnterpriseEntityLocal second = home.findByPrimaryKey("Second");
         first.setNext(second);
         second.setNext(first);
      }
      catch (Exception e)
      {
         throw new EJBException("Unable to create data", e);
      }
   }

   public void cmrTest(String jndiName, String start)
   {
      try
      {
         InitialContext ctx = new InitialContext();
         EnterpriseEntityLocalHome home = (EnterpriseEntityLocalHome)ctx.lookup(jndiName);
         EnterpriseEntityLocal initial = home.findByPrimaryKey(start);
         initial.getNext().getName();
      }
      catch (Exception e)
      {
         throw new EJBException("Unable to create data", e);
      }
   }
}
