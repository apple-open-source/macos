package org.jboss.test.testbean.interfaces;

import javax.ejb.EJBObject;
import java.rmi.RemoteException;

public interface EntityBMP extends EJBObject {

  public String callBusinessMethodA() throws RemoteException;

  public String callBusinessMethodB() throws RemoteException;
  
  public String callBusinessMethodB(String words) throws RemoteException;
}
