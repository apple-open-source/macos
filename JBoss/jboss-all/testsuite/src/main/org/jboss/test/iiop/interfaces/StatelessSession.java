/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.iiop.interfaces;

import java.rmi.RemoteException;
import javax.ejb.EJBObject;

/**
 *   @author reverbel@ime.usp.br
 *   @version $Revision: 1.1.2.2 $
 */
public interface StatelessSession
   extends EJBObject
{
   public String getString()
      throws java.rmi.RemoteException;
   
   public String testPrimitiveTypes(boolean flag, char c, byte b,
                                    short s, int i, long l, float f, double d)
      throws java.rmi.RemoteException;
   
   public String testString(String s)
      throws java.rmi.RemoteException;
   
   public StatelessSession testStatelessSession(String s, StatelessSession t)
      throws java.rmi.RemoteException;
   
   public java.rmi.Remote testRemote(String s, java.rmi.Remote t)
      throws java.rmi.RemoteException;
   
   public Foo testSerializable(Foo foo)
      throws java.rmi.RemoteException;
   
   public int[] testIntArray(int[] a)
      throws java.rmi.RemoteException;
   
   public Foo[] testValueArray(Foo[] a)
      throws java.rmi.RemoteException;
   
   public String testException(int i)
      throws NegativeArgumentException, java.rmi.RemoteException;
   
   public Object fooValueToObject(Foo foo)
      throws java.rmi.RemoteException;
   
   public Object booValueToObject(Boo boo)
      throws java.rmi.RemoteException;
   
   public java.util.Vector valueArrayToVector(Foo[] a)
      throws java.rmi.RemoteException;
   
   public Foo[] vectorToValueArray(java.util.Vector v)
      throws java.rmi.RemoteException;
   
   public Object getException()
      throws java.rmi.RemoteException;
   
   public Object getZooValue()
      throws java.rmi.RemoteException;
   
   public Object[] testReferenceSharingWithinArray(Object[] a)
      throws java.rmi.RemoteException;
   
   public java.util.Collection testReferenceSharingWithinCollection(
                                                        java.util.Collection c)
      throws java.rmi.RemoteException;

   public org.omg.CORBA.Object testCorbaObject(org.omg.CORBA.Object obj)
      throws java.rmi.RemoteException;

   public IdlInterface testIdlInterface(IdlInterface ref)
      throws java.rmi.RemoteException;

}
