
//Title:        telkel
//Version:      
//Copyright:    Copyright (c) 1999
//Author:       Marc Fleury
//Company:      telkel
//Description:  Your description

package org.jboss.test.testbean.interfaces;

import javax.ejb.*;
import java.rmi.*;


public interface StatelessSession extends EJBObject {

  public void callBusinessMethodA() throws RemoteException;

  public String callBusinessMethodB() throws RemoteException;

  public String callBusinessMethodB(String words) throws RemoteException;

  public String callBusinessMethodC() throws RemoteException;

  public void callBusinessMethodD() throws RemoteException, BusinessMethodException;
  
  public String callBusinessMethodE() throws RemoteException;
  
  public void testClassLoading() throws RemoteException, BusinessMethodException;
}
