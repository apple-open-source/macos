package org.jboss.test.testbean.interfaces;

         
import javax.ejb.*;
import java.rmi.*;


public interface BMTStatelessHome extends EJBHome {
    
  public BMTStateless create() throws java.rmi.RemoteException, javax.ejb.CreateException;
} 

