package org.jboss.test.tomcat.ejb.interfaces;

import javax.ejb.*;
import java.rmi.*;

public interface StatelessSessionHome extends EJBHome {

  public StatelessSession create() throws java.rmi.RemoteException, javax.ejb.CreateException;
} 
