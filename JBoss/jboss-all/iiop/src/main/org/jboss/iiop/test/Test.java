/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.iiop.test;

import javax.ejb.EJBObject;
import java.rmi.RemoteException;

/**
 */
//public interface Test extends AbstractTestBase, TestBase {
public interface Test extends TestBase {
        public final String const2 = "abc";

   public Test aa1() throws RemoteException;
   public TestValue aa2() throws RemoteException;
   public Object aa3() throws RemoteException;
   public java.io.Serializable aa4() throws RemoteException;
   public java.io.Externalizable aa5() throws RemoteException;
   public java.rmi.Remote aa6() throws RemoteException;
   
   public String jack(String arg) throws RemoteException;
   public String Jack(String arg) throws RemoteException;
   public String jAcK(String arg) throws RemoteException;
   
   /**
    * Gets the current value of the autonumber.
    */
   public int getValue() throws RemoteException;
   
   /**
    * Sets the current value of the autonumber.
    */
   public void setValue(int value) throws RemoteException;
   
   /**
    * A test operation.
    */
   public TestValue[][] addNumbers(int[] numbers,
                                   boolean b, char c, byte by, short s,
                                   int i, long l, float f, double d,
                                   java.rmi.Remote rem,
                                   TestValue val, Test intf,
                                   String str, Object obj, Class cls,
                                   java.io.Serializable ser,
                                   java.io.Externalizable ext)
      throws TestException, RemoteException;
}
