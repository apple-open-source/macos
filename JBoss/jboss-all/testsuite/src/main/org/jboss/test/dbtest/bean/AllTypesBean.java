package org.jboss.test.dbtest.bean;

import java.rmi.*;
import javax.ejb.*;
import java.util.Collection;
import java.util.ArrayList;

import javax.naming.Context;
import javax.naming.InitialContext;
import java.sql.Date;
import java.sql.Time;
import java.sql.Timestamp;
import org.jboss.test.dbtest.interfaces.MyObject;


public class AllTypesBean implements EntityBean {
    public boolean aBoolean;
	public byte aByte;
	public short aShort;
    public int anInt;
    public long aLong;
	public float aFloat;
	public double aDouble;
//    public char aChar;
    public String aString;
    public Date aDate;
    public Time aTime;
	public Timestamp aTimestamp;

	public MyObject anObject;

	public Collection aList;

	private EntityContext entityContext;


    public String ejbCreate(String pk) throws RemoteException, CreateException {
		return ejbCreate(true, (byte)1, (short)2, (int)3, (long)4, (float)5.6,
			(double)7.8, /*'9',*/ pk, new Date(System.currentTimeMillis()),
	    	new Time(System.currentTimeMillis()),
            new Timestamp(System.currentTimeMillis()), new MyObject());
	}

    public void ejbPostCreate(String pk)
		throws RemoteException, CreateException {}


    public String ejbCreate(boolean aBoolean, byte aByte, short aShort, int anInt,
		long aLong, float aFloat, double aDouble, /*char aChar,*/ String aString,
		Date aDate, Time aTime, Timestamp aTimestamp, MyObject anObject )

		throws RemoteException, CreateException {

        this.aBoolean = aBoolean;
		this.aByte = aByte;
		this.aShort = aShort;
    	this.anInt = anInt;
		this.aLong = aLong;
		this.aFloat = aFloat;
		this.aDouble = aDouble;
		//this.aChar = aChar;
        this.aString = aString;
		this.aDate = aDate;
        this.aTime = aTime;
		this.aTimestamp = aTimestamp;
		this.anObject = anObject;

		aList = new ArrayList();

        return null;
    }


    public void ejbPostCreate(boolean aBoolean, byte aByte, short aShort, int anInt,
		long aLong, float aFloat, double aDouble, /*char aChar,*/ String aString,
		Date aDate, Time aTime, Timestamp aTimestamp, MyObject anObject )

		throws RemoteException, CreateException {}

    public void ejbActivate() throws RemoteException {}

    public void ejbLoad() throws RemoteException {}

    public void ejbPassivate() throws RemoteException {}

    public void ejbRemove() throws RemoteException, RemoveException {}

    public void ejbStore() throws RemoteException {}


    public void setEntityContext(EntityContext context) throws RemoteException {
        entityContext = context;
    }

    public void unsetEntityContext() throws RemoteException {
        entityContext = null;
    }


    public void updateAllValues(boolean aBoolean, byte aByte, short aShort, int anInt,
		long aLong, float aFloat, double aDouble, /*char aChar,*/ String aString,
		Date aDate, Time aTime, Timestamp aTimestamp, MyObject anObject ) {

        this.aBoolean = aBoolean;
		this.aByte = aByte;
		this.aShort = aShort;
    	this.anInt = anInt;
		this.aLong = aLong;
		this.aFloat = aFloat;
		this.aDouble = aDouble;
		//this.aChar = aChar;
        this.aString = aString;
		this.aDate = aDate;
        this.aTime = aTime;
		this.aTimestamp = aTimestamp;
		this.anObject = anObject;

    }

	public void addObjectToList(Object anObject) throws RemoteException {
		aList.add(anObject);
	}

	public void removeObjectFromList(Object anObject) throws RemoteException {
		aList.remove(anObject);
	}

    public Collection getObjectList() throws RemoteException { return aList; }

	public boolean getBoolean() throws RemoteException { return aBoolean; }
    public byte getByte() throws RemoteException { return aByte; }
	public short getShort() throws RemoteException { return aShort; }
	public int getInt() throws RemoteException { return anInt; }
	public long getLong() throws RemoteException { return aLong; }
	public float getFloat() throws RemoteException { return aFloat; }
	public double getDouble() throws RemoteException { return aDouble; }
	//public char getChar() throws RemoteException { return aChar; }
	public String getString() throws RemoteException { return aString; }
	public Date getDate() throws RemoteException { return aDate; }
	public Time getTime() throws RemoteException { return aTime; }
	public Timestamp getTimestamp() throws RemoteException { return aTimestamp; }

	public MyObject getObject() throws RemoteException { return anObject; }

    public void setByte(byte b) {aByte = b;}
    public void setShort(short s) {aShort = s;}
    public void setInt(int i) {anInt = i;}
    public void setLong(long l) {aLong = l;}
    public void setFloat(float f) {aFloat = f;}
    public void setDouble(double d) {aDouble = d;}
}
