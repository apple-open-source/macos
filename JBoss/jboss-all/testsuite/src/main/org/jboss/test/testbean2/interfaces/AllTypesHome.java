package org.jboss.test.testbean2.interfaces;


import javax.ejb.EJBHome;
import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.FinderException;
import java.util.Collection;
import javax.ejb.Handle;
import java.sql.Date;
import java.sql.Timestamp;

import org.jboss.test.testbean.interfaces.EnterpriseEntity;
import org.jboss.test.testbean.interfaces.StatefulSession;
import org.jboss.test.testbean.interfaces.StatelessSession;

public interface AllTypesHome extends EJBHome {

    public AllTypes create(String pk) throws RemoteException, CreateException; 
	
	public AllTypes create(boolean aBoolean, byte aByte, short aShort, int anInt, 
		long aLong, float aFloat, double aDouble, /*char aChar,*/ String aString, 
		Date aDate, Timestamp aTimestamp, MyObject anObject )
		
        throws RemoteException, CreateException;


	// automatically generated finders
	public AllTypes findByPrimaryKey(String name)
        throws RemoteException, FinderException;

    public Collection findAll()
        throws RemoteException, FinderException;
	    
	public Collection findByABoolean(boolean b)
        throws RemoteException, FinderException;
	    
	public Collection findByAByte(byte b)
        throws RemoteException, FinderException;
	    
	public Collection findByAShort(short s)
        throws RemoteException, FinderException;
	    
	public Collection findByAnInt(int i)
        throws RemoteException, FinderException;
	    
	public Collection findByALong(long l)
        throws RemoteException, FinderException;
	    
	public Collection findByAFloat(float f)
        throws RemoteException, FinderException;
	    
	public Collection findByADouble(double d)
        throws RemoteException, FinderException;
	    
//	public Collection findByAChar(char c)
//        throws RemoteException, FinderException;
	    
	public Collection findByAString(String s)
        throws RemoteException, FinderException;
	    
	public Collection findByADate(Date d)
        throws RemoteException, FinderException;
	    
	public Collection findByATimestamp(Timestamp t)
        throws RemoteException, FinderException;
	    
	public Collection findByAnObject(MyObject o)
        throws RemoteException, FinderException;
	    
	public Collection findByEnterpriseEntity(EnterpriseEntity e)
        throws RemoteException, FinderException;
	
	public Collection findByStatefulSession(StatefulSession s)
        throws RemoteException, FinderException;

	public Collection findByStatelessSession(StatelessSession s)
        throws RemoteException, FinderException;
	    
	
	// finders defined in jaws.xml
	public Collection findByMinInt(int min)
        throws RemoteException, FinderException;
	    
	public Collection findByIntAndDouble(int i, double d)
        throws RemoteException, FinderException;
	    
	
}
