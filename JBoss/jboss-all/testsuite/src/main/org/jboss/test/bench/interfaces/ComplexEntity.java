package org.jboss.test.bench.interfaces;

import java.rmi.RemoteException;
import javax.ejb.EJBObject;

import org.jboss.test.bench.interfaces.AComplexPK;

public interface ComplexEntity extends EJBObject {
   
   public String getOtherField() throws RemoteException;
   
   public void setOtherField(String otherField) throws RemoteException;
}
