package org.jboss.test.bench.interfaces;

import javax.ejb.*;
import java.rmi.*;

public interface MySession extends EJBObject {

  public int getInt() throws RemoteException;

} 

