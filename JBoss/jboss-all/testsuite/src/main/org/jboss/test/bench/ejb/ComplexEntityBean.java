package org.jboss.test.bench.ejb;

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;

import org.jboss.test.bench.interfaces.AComplexPK;

public class ComplexEntityBean implements EntityBean {
    public boolean aBoolean;
    public int anInt;
    public long aLong;
    public double aDouble;
    public String aString;

	public String otherField;
   
	public AComplexPK ejbCreate(boolean aBoolean, int anInt, long aLong, double aDouble, String aString) throws RemoteException, CreateException {

        this.aBoolean = aBoolean;
        this.anInt = anInt;
        this.aLong = aLong;
        this.aDouble = aDouble;
        this.aString = aString;

		this.otherField = "";
		return null;
	}
   
	public void ejbPostCreate(boolean aBoolean, int anInt, long aLong, double aDouble, String aString) throws RemoteException, CreateException {}
      
	public String getOtherField() throws RemoteException {
		return otherField;
	}
   
	public void setOtherField(String otherField) throws RemoteException {
		this.otherField = otherField;
	}

	public void ejbStore() throws RemoteException {}

	public void ejbLoad() throws RemoteException {}
	
	public void ejbActivate() throws RemoteException {}
	
	public void ejbPassivate() throws RemoteException {}
	
	public void ejbRemove() throws RemoteException {}

	public void setEntityContext(EntityContext e) throws RemoteException {}

	public void unsetEntityContext() throws RemoteException {}

}
