package org.jboss.test.testbean.interfaces;

         
import javax.ejb.*;
import java.rmi.*;


public interface BMTStatefulHome extends EJBHome {
    
  public BMTStateful create() throws java.rmi.RemoteException, javax.ejb.CreateException;
  public BMTStateful create(String caca) throws java.rmi.RemoteException, javax.ejb.CreateException;
  public BMTStateful create(String caca, String cacaprout) throws java.rmi.RemoteException, javax.ejb.CreateException;

} 

