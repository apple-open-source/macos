package org.jboss.test.bench.interfaces;

import java.rmi.RemoteException;
import javax.ejb.EJBObject;

public interface SimpleEntity extends EJBObject {
   
   public int getField() throws RemoteException;
   public void setField(int field) throws RemoteException;
}
