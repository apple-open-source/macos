package org.jboss.test.testbean2.bean;

import java.rmi.*;
import javax.ejb.*;
import java.util.Collection;
import java.util.ArrayList;

import org.jboss.test.testbean.interfaces.StatefulSession;
import org.jboss.test.testbean.interfaces.StatefulSessionHome;
import org.jboss.test.testbean.interfaces.StatelessSession;
import org.jboss.test.testbean.interfaces.StatelessSessionHome;
import org.jboss.test.testbean.interfaces.EnterpriseEntity;
import org.jboss.test.testbean.interfaces.EnterpriseEntityHome;
import javax.naming.Context;
import javax.naming.InitialContext;
import java.sql.Date;
import java.sql.Timestamp;
import org.jboss.test.testbean2.interfaces.MyObject;


public class AllTypesBean implements EntityBean {

       static org.apache.log4j.Category log =
       org.apache.log4j.Category.getInstance(AllTypesBean.class);

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
   public Timestamp aTimestamp;
   
   public MyObject anObject;
   
   public StatefulSession statefulSession;
   public StatelessSession statelessSession;
   public EnterpriseEntity enterpriseEntity;
   
   public Collection aList;
   
   private EntityContext entityContext;
   
   
   public String ejbCreate(String pk) throws RemoteException, CreateException {
      return ejbCreate(true, (byte)1, (short)2, (int)3, (long)4, (float)5.6, 
         (double)7.8, /*'9',*/ pk, new Date(System.currentTimeMillis()),
         new Timestamp(System.currentTimeMillis()), new MyObject());
   }
   
   public void ejbPostCreate(String pk)		 
   throws RemoteException, CreateException {}
   
   
   public String ejbCreate(boolean aBoolean, byte aByte, short aShort, int anInt, 
      long aLong, float aFloat, double aDouble, /*char aChar,*/ String aString, 
      Date aDate, Timestamp aTimestamp, MyObject anObject ) 
   
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
      this.aTimestamp = aTimestamp;
      this.anObject = anObject;
      
      try {
         Context ctx = new InitialContext();
         
         StatefulSessionHome sfHome = (StatefulSessionHome)ctx.lookup("java:comp/env/ejb/stateful");
         statefulSession = sfHome.create();
         
         StatelessSessionHome slHome = (StatelessSessionHome)ctx.lookup("java:comp/env/ejb/stateless");
         statelessSession = slHome.create();
         
         EnterpriseEntityHome eeHome = (EnterpriseEntityHome)ctx.lookup("java:comp/env/ejb/entity");
         try {
            enterpriseEntity = eeHome.findByPrimaryKey(aString);
         } catch (FinderException e) {
            enterpriseEntity = eeHome.create(aString);
         }
      
      } catch (Exception e) {
         log.debug("failed", e);
         throw new CreateException(e.getMessage());
      }
      
      aList = new ArrayList();
      
      return null;
   }
   
   
   public void ejbPostCreate(boolean aBoolean, byte aByte, short aShort, int anInt, 
      long aLong, float aFloat, double aDouble, /*char aChar,*/ String aString, 
      Date aDate, Timestamp aTimestamp, MyObject anObject ) 
   
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
   
   
   public String callBusinessMethodA() throws RemoteException {
      // test external ejb-ref in testbeans.jar
      return statefulSession.callBusinessMethodA();
   }
   
   
   
   public void updateAllValues(boolean aBoolean, byte aByte, short aShort, int anInt, 
      long aLong, float aFloat, double aDouble, /*char aChar,*/ String aString, 
      Date aDate, Timestamp aTimestamp, MyObject anObject ) {
      
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
      this.aTimestamp = aTimestamp;
      this.anObject = anObject;
      
      try {
         Context ctx = new InitialContext();
         
         EnterpriseEntityHome eeHome = (EnterpriseEntityHome)ctx.lookup("java:comp/env/ejb/entity");
         try {
            enterpriseEntity = eeHome.findByPrimaryKey(aString);
         } catch (FinderException e) {
            enterpriseEntity = eeHome.create(aString);
         }
      
      } catch (Exception e) {
         // ignore
      }
   
   }
   
   /**
    * @todo Remove creation of new list when value object correctly
    * implement pass by value
    */
   public void addObjectToList(Object anObject) throws RemoteException {
      aList = new ArrayList(aList);
      aList.add(anObject);
   }
   
   /**
    * @todo Remove creation of new list when value object correctly
    * implement pass by value
    */
   public void removeObjectFromList(Object anObject) throws RemoteException {
      aList = new ArrayList(aList);
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
   public Timestamp getTimestamp() throws RemoteException { return aTimestamp; }
   
   public MyObject getObject() throws RemoteException { return anObject; }
   
   public Handle getStateful() throws RemoteException { 
      return statefulSession.getHandle();
   }
   
   public Handle getStateless() throws RemoteException {
      return statelessSession.getHandle();
   }
   
   public Handle getEntity() throws RemoteException {
      return enterpriseEntity.getHandle();
   }


}
