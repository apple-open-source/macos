package org.jboss.test.dbtest.interfaces;


import javax.ejb.EJBHome;
import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.FinderException;
import java.util.Collection;
import java.sql.Date;
import java.sql.Time;
import java.sql.Timestamp;


public interface AllTypesHome extends EJBHome {

	public AllTypes create(String pk) throws RemoteException, CreateException;

	public AllTypes create(boolean aBoolean, byte aByte, short aShort, int anInt,
		long aLong, float aFloat, double aDouble, /*char aChar,*/ String aString,
		Date aDate, Time aTime, Timestamp aTimestamp, MyObject anObject )

	throws RemoteException, CreateException;


	// automatically generated finders
	public AllTypes findByPrimaryKey(String name)
	throws RemoteException, FinderException;

	public Collection findAll()
	throws RemoteException, FinderException;

}
