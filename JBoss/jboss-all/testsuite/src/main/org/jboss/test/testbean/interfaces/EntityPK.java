package org.jboss.test.testbean.interfaces;

import javax.ejb.EJBObject;
import java.rmi.RemoteException;



public interface EntityPK extends EJBObject {

    public AComplexPK readAllValues() throws RemoteException;
    public void updateAllValues(AComplexPK aComplexPK) throws RemoteException;
	
	public int getOtherField() throws RemoteException;
	public void setOtherField(int newValue) throws RemoteException;
	
}
