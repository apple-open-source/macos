
//Title:        telkel
//Version:
//Copyright:    Copyright (c) 1999
//Author:       Marc Fleury
//Company:      telkel
//Description:  Your description

package org.jboss.test.testbeancluster.bean;

import java.rmi.*;
import javax.ejb.*;
import javax.naming.InitialContext;
import javax.naming.Context;
import org.jboss.test.testbean.interfaces.*;

public class StatelessSessionBean extends org.jboss.test.testbean.bean.StatelessSessionBean 
{
   
   public static long numberOfCalls = 0;
   
   public String callBusinessMethodB()
   {
      String rtn = super.callBusinessMethodB();
      testColocation();
      return rtn;
   }

   public void testColocation()
   {
      try
      {
         System.out.println("begin testColocation");
         InitialContext ctx = new InitialContext();
         StatelessSessionHome home = (StatelessSessionHome)ctx.lookup("nextgen.StatelessSession");
         StatelessSession session = home.create();
         session.callBusinessMethodA();
         System.out.println("end testColocation");
      }
      catch (Exception ex)
      {
         ex.printStackTrace();
      }

   }
   
   public void resetNumberOfCalls ()
   {
      System.out.println("Number of calls has been reseted");
      numberOfCalls = 0;
   }
   
   public void makeCountedCall ()
   {
      System.out.println("makeCountedCall called");
      numberOfCalls++;
   }
   
   public long getCallCount ()
   {
      System.out.println("getCallCount called");
      return numberOfCalls;
   }
   
}
