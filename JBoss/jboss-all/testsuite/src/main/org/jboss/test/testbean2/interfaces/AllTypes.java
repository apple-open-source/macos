package org.jboss.test.testbean2.interfaces;


import javax.ejb.EJBObject;
import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.FinderException;
import java.util.Collection;
import javax.ejb.Handle;
import java.sql.Date;
import java.sql.Timestamp;

public interface AllTypes extends EJBObject {

	// business methods
	public void updateAllValues(boolean aBoolean, byte aByte, short aShort, int anInt, 
		long aLong, float aFloat, double aDouble, /*char aChar,*/ String aString, 
		Date aDate, Timestamp aTimestamp, MyObject anObject ) throws RemoteException;
        
	public void addObjectToList(Object anObject) throws RemoteException;
	public void removeObjectFromList(Object anObject) throws RemoteException;
	public Collection getObjectList() throws RemoteException;
	                                   	
	public String callBusinessMethodA() throws RemoteException;
	
	public boolean getBoolean() throws RemoteException;
    public byte getByte() throws RemoteException;
	public short getShort() throws RemoteException;
	public int getInt() throws RemoteException;
	public long getLong() throws RemoteException;
	public float getFloat() throws RemoteException;
	public double getDouble() throws RemoteException;
	//public char getChar() throws RemoteException;
	public String getString() throws RemoteException;
	public Date getDate() throws RemoteException;
	public Timestamp getTimestamp() throws RemoteException;
	
	public MyObject getObject() throws RemoteException;
	
	public Handle getStateful() throws RemoteException;
	public Handle getStateless() throws RemoteException;
	public Handle getEntity() throws RemoteException;
	
	
}
