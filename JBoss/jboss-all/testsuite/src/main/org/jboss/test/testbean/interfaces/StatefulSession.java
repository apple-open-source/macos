
//Title:        telkel
//Version:      
//Copyright:    Copyright (c) 1999
//Author:       Marc Fleury
//Company:      telkel
//Description:  Your description

package org.jboss.test.testbean.interfaces;

import javax.ejb.*;
import java.rmi.*;

public interface StatefulSession extends EJBObject {

  public String callBusinessMethodA() throws RemoteException;

  public String callBusinessMethodB() throws RemoteException;
  
  public String callBusinessMethodB(String words) throws RemoteException;
} 

