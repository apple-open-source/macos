
//Title:        telkel
//Version:      
//Copyright:    Copyright (c) 1999
//Author:       Marc Fleury
//Company:      telkel
//Description:  Your description

package org.jboss.test.deadlock.interfaces;

import javax.ejb.*;
import java.rmi.*;


public interface StatelessSession extends EJBObject {

   public void callAB() throws RemoteException;
   public void callBA() throws RemoteException;
   public void requiresNewTest(boolean first) throws RemoteException;
   public void createCMRTestData(String jndiName) throws RemoteException;
   public void cmrTest(String jndiName, String start) throws RemoteException;
}
