/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.iiop.test;

import java.rmi.RemoteException;

public class TestImpl
   implements Test
{
public Test aa1() throws RemoteException
{ return null; }

public TestValue aa2() throws RemoteException
{ return null; }

public Object aa3() throws RemoteException
{ return null; }

public java.io.Serializable aa4() throws RemoteException
{ return null; }

public java.io.Externalizable aa5() throws RemoteException
{ return null; }

public java.rmi.Remote aa6() throws RemoteException
{ return null; }

public String jack(String arg) throws RemoteException
{ return "jack:" + arg; }

public String Jack(String arg) throws RemoteException
{ return "Jack:" + arg; }

public String jAcK(String arg) throws RemoteException
{ return "jAcK:" + arg; }

        /**
         * Accessor of type Object
         */
        public Object getObjectValue() throws RemoteException
        {
            return null;
        }


        /**
         * Gets the current value of the autonumber.
         */
        public int getValue() throws RemoteException
        {
            return 0;
        }
 
        /**
         * Sets the current value of the autonumber.
         */
        public void setValue(int value) throws RemoteException
        {
        }
 
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
            throws TestException, RemoteException
        {
            return null;
        }
}
