/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.iiop.test;


import java.rmi.Remote;
import javax.naming.InitialContext;
import javax.rmi.PortableRemoteObject;
import junit.framework.Assert;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;
import org.omg.CORBA.ORB;
import org.omg.PortableServer.POA;
import org.omg.PortableServer.POAHelper;
import org.jboss.test.JBossTestCase;
import org.jboss.test.iiop.interfaces.Boo;
import org.jboss.test.iiop.interfaces.Foo;
import org.jboss.test.iiop.interfaces.IdlInterface;
import org.jboss.test.iiop.interfaces.IdlInterfaceHelper;
import org.jboss.test.iiop.interfaces.NegativeArgumentException;
import org.jboss.test.iiop.interfaces.StatelessSession;
import org.jboss.test.iiop.interfaces.StatelessSessionHome;
import org.jboss.test.iiop.interfaces.Zoo;
import org.jboss.test.iiop.util.Util;

/**
 *   @author reverbel@ime.usp.br
 *   @version $Revision: 1.1.2.2 $
 */
public class ParameterPassingStressTestCase
   extends JBossTestCase
{
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   private java.util.Properties jndiProps;
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   public ParameterPassingStressTestCase(String name) 
       throws java.io.IOException
   {
      super(name);
      java.net.URL url 
          = ClassLoader.getSystemResource("cosnaming.jndi.properties");
      jndiProps = new java.util.Properties();
      jndiProps.load(url.openStream());
   }
   
   // Override getInitialContext() ----------------------------------
   protected InitialContext getInitialContext() throws Exception
   {
      return new InitialContext(jndiProps);
   }

   // Public --------------------------------------------------------

   public void test_getString()
      throws Exception
   {
      StatelessSessionHome home = 
         (StatelessSessionHome)PortableRemoteObject.narrow(
                    getInitialContext().lookup(StatelessSessionHome.JNDI_NAME),
                    StatelessSessionHome.class);
      StatelessSession session = home.create();
      //----------------------------------------------------------------------
      String s = session.getString();
      assertEquals(Util.STRING, s);
      //----------------------------------------------------------------------
      session.remove();
   }

   public void test_PrimitiveTypes()
      throws Exception
   {
      StatelessSessionHome home = 
         (StatelessSessionHome)PortableRemoteObject.narrow(
                    getInitialContext().lookup(StatelessSessionHome.JNDI_NAME),
                    StatelessSessionHome.class);
      StatelessSession session = home.create();
      //----------------------------------------------------------------------
      String s;
      s = session.testPrimitiveTypes(false,
                                     'A',
                                     Byte.MIN_VALUE,
                                     Short.MIN_VALUE,
                                     Integer.MIN_VALUE,
                                     Long.MIN_VALUE,
                                     Float.MIN_VALUE,
                                     Double.MIN_VALUE);
      assertEquals(Util.primitiveTypesToString(false,
                                               'A',
                                               Byte.MIN_VALUE,
                                               Short.MIN_VALUE,
                                               Integer.MIN_VALUE,
                                               Long.MIN_VALUE,
                                               Float.MIN_VALUE,
                                               Double.MIN_VALUE),
                   s);
      s = session.testPrimitiveTypes(true,
                                     'Z',
                                     Byte.MAX_VALUE,
                                     Short.MAX_VALUE,
                                     Integer.MAX_VALUE,
                                     Long.MAX_VALUE,
                                     Float.MAX_VALUE,
                                     Double.MAX_VALUE);
      assertEquals(Util.primitiveTypesToString(true,
                                               'Z',
                                               Byte.MAX_VALUE,
                                               Short.MAX_VALUE,
                                               Integer.MAX_VALUE,
                                               Long.MAX_VALUE,
                                               Float.MAX_VALUE,
                                               Double.MAX_VALUE),
                   s);
      //----------------------------------------------------------------------
      session.remove();
   }

   public void test_String()
      throws Exception
   {
      StatelessSessionHome home = 
         (StatelessSessionHome)PortableRemoteObject.narrow(
                    getInitialContext().lookup(StatelessSessionHome.JNDI_NAME),
                    StatelessSessionHome.class);
      StatelessSession session = home.create();
      //----------------------------------------------------------------------
      String original = "0123456789";
      String echoedBack = session.testString(original);
      assertEquals(Util.echo(original), echoedBack);
      //----------------------------------------------------------------------
      session.remove();
   }

   public void test_StatelessSession()
      throws Exception
   {
      StatelessSessionHome home = 
         (StatelessSessionHome)PortableRemoteObject.narrow(
                    getInitialContext().lookup(StatelessSessionHome.JNDI_NAME),
                    StatelessSessionHome.class);
      StatelessSession session = home.create();
      //----------------------------------------------------------------------
      StatelessSession session2 =
         session.testStatelessSession("the quick brown fox", session);
      String s = session2.getString();
      assertEquals(Util.STRING, s);
      //----------------------------------------------------------------------
      session.remove();
   }
   
   public void test_Remote()
      throws Exception
   {
      StatelessSessionHome home = 
         (StatelessSessionHome)PortableRemoteObject.narrow(
                    getInitialContext().lookup(StatelessSessionHome.JNDI_NAME),
                    StatelessSessionHome.class);
      StatelessSession session = home.create();
      //----------------------------------------------------------------------
      Remote r = session.testRemote("jumps over the lazy dog", session);
      StatelessSession session2 =
         (StatelessSession)PortableRemoteObject.narrow(r,
                                                       StatelessSession.class);
      String s = session2.getString();
      assertEquals(Util.STRING, s);
      //----------------------------------------------------------------------
      session.remove();
   }
   
   public void test_Serializable()
      throws Exception
   {
      StatelessSessionHome home = 
         (StatelessSessionHome)PortableRemoteObject.narrow(
                    getInitialContext().lookup(StatelessSessionHome.JNDI_NAME),
                    StatelessSessionHome.class);
      StatelessSession session = home.create();
      //----------------------------------------------------------------------
      Foo original = new Foo(7, "foo test");
      Foo echoedBack = session.testSerializable(original);
      assertEquals(Util.echoFoo(original), echoedBack);
      //----------------------------------------------------------------------
      session.remove();
   }
   
   public void test_intArray()
      throws Exception
   {
      StatelessSessionHome home = 
         (StatelessSessionHome)PortableRemoteObject.narrow(
                    getInitialContext().lookup(StatelessSessionHome.JNDI_NAME),
                    StatelessSessionHome.class);
      StatelessSession session = home.create();
      //----------------------------------------------------------------------
      int[] original= new int[10];
      for (int i = 0; i < original.length; i++)
         original[i] = 100 + i;
      int[] echoedBack = session.testIntArray(original);
      assertEquals(original.length, echoedBack.length);
      for (int i = 0; i < echoedBack.length; i++)
         assertEquals(original[i] + 1, echoedBack[i]);
      //----------------------------------------------------------------------
      session.remove();
   }
   
   public void test_valueArray()
      throws Exception
   {
      StatelessSessionHome home = 
         (StatelessSessionHome)PortableRemoteObject.narrow(
                    getInitialContext().lookup(StatelessSessionHome.JNDI_NAME),
                    StatelessSessionHome.class);
      StatelessSession session = home.create();
      //----------------------------------------------------------------------
      Foo[] original = new Foo[4];
      for (int i = 0; i < original.length; i++)
         original[i] = new Foo(100 + i, "foo array test");
      Foo[] echoedBack = session.testValueArray(original);
      assertEquals(original.length, echoedBack.length);
      for (int i = 0; i < echoedBack.length; i++)
         assertEquals(Util.echoFoo(original[i]), echoedBack[i]);
      //----------------------------------------------------------------------
      session.remove();
   }
   
   public void test_exception()
      throws Exception
   {
      StatelessSessionHome home = 
         (StatelessSessionHome)PortableRemoteObject.narrow(
                    getInitialContext().lookup(StatelessSessionHome.JNDI_NAME),
                    StatelessSessionHome.class);
      StatelessSession session = home.create();
      //----------------------------------------------------------------------
      assertEquals("#0", session.testException(0));
      assertEquals("#1", session.testException(1));
      assertEquals("#2", session.testException(2));
      try
      {
         session.testException(-2);
         fail("NegativeArgumentException expected but not thrown.");
      }
      catch (NegativeArgumentException na)
      {
         assertEquals(-2, na.getNegativeArgument());
      }
      try
      {
         session.testException(-1);
         fail("NegativeArgumentException expected but not thrown.");
      }
      catch (NegativeArgumentException na)
      {
         assertEquals(-1, na.getNegativeArgument());
      }
      assertEquals("#0", session.testException(0));
      //----------------------------------------------------------------------
      session.remove();
   }
   
   public void test_FooValueToObject()
      throws Exception
   {
      StatelessSessionHome home = 
         (StatelessSessionHome)PortableRemoteObject.narrow(
                    getInitialContext().lookup(StatelessSessionHome.JNDI_NAME),
                    StatelessSessionHome.class);
      StatelessSession session = home.create();
      //----------------------------------------------------------------------
      Foo original = new Foo(9999, "foo test");
      java.lang.Object echoedBack = session.fooValueToObject(original);
      assertEquals(Util.echoFoo(original), echoedBack);
      //----------------------------------------------------------------------
      session.remove();
   }
   
   public void test_BooValueToObject()
      throws Exception
   {
      StatelessSessionHome home = 
         (StatelessSessionHome)PortableRemoteObject.narrow(
                    getInitialContext().lookup(StatelessSessionHome.JNDI_NAME),
                    StatelessSessionHome.class);
      StatelessSession session = home.create();
      //----------------------------------------------------------------------
      Boo original = new Boo("t1", "boo test");
      java.lang.Object echoedBack = session.booValueToObject(original);
      assertEquals(Util.echoBoo(original), echoedBack);
      //----------------------------------------------------------------------
      session.remove();
   }
   
   public void test_valueArrayToVector()
      throws Exception
   {
      StatelessSessionHome home = 
         (StatelessSessionHome)PortableRemoteObject.narrow(
                    getInitialContext().lookup(StatelessSessionHome.JNDI_NAME),
                    StatelessSessionHome.class);
      StatelessSession session = home.create();
      //----------------------------------------------------------------------
      Foo[] original = new Foo[4];
      for (int i = 0; i < original.length; i++)
         original[i] = new Foo(100 + i, "foo vector test");
      java.util.Vector v = session.valueArrayToVector(original);
      java.lang.Object[] echoedBack = v.toArray();
      assertEquals(original.length, echoedBack.length);
      for (int i = 0; i < echoedBack.length; i++)
         assertEquals(Util.echoFoo(original[i]), echoedBack[i]);
      //----------------------------------------------------------------------
      session.remove();
   }
   
   public void test_vectorToValueArray()
      throws Exception
   {
      StatelessSessionHome home = 
         (StatelessSessionHome)PortableRemoteObject.narrow(
                    getInitialContext().lookup(StatelessSessionHome.JNDI_NAME),
                    StatelessSessionHome.class);
      StatelessSession session = home.create();
      //----------------------------------------------------------------------
      Foo[] original = new Foo[4];
      for (int i = 0; i < original.length; i++)
         original[i] = new Foo(100 + i, "foo vector test");
      java.util.Vector v = session.valueArrayToVector(original);
      Foo[] echoedBack = session.vectorToValueArray(v);
      assertEquals(original.length, echoedBack.length);
      for (int i = 0; i < echoedBack.length; i++)
         assertEquals(Util.echoFoo(Util.echoFoo(original[i])),
                      echoedBack[i]);
      //----------------------------------------------------------------------
      session.remove();
   }
   
   public void test_getException()
      throws Exception
   {
      StatelessSessionHome home = 
         (StatelessSessionHome)PortableRemoteObject.narrow(
                    getInitialContext().lookup(StatelessSessionHome.JNDI_NAME),
                    StatelessSessionHome.class);
      StatelessSession session = home.create();
      //----------------------------------------------------------------------
      java.lang.Object obj = session.getException();
      NegativeArgumentException na = (NegativeArgumentException)obj;
      assertEquals(-7777, na.getNegativeArgument());
      //----------------------------------------------------------------------
      session.remove();
   }
   
   public void test_getZooValue()
      throws Exception
   {
      StatelessSessionHome home = 
         (StatelessSessionHome)PortableRemoteObject.narrow(
                    getInitialContext().lookup(StatelessSessionHome.JNDI_NAME),
                    StatelessSessionHome.class);
      StatelessSession session = home.create();
      //----------------------------------------------------------------------
      java.lang.Object obj = session.getZooValue();
      Assert.assertEquals(new Zoo("outer_zoo!",
                                  "returned by getZooValue",
                                  new Zoo("inner_zoo!", "inner")),
                          obj);
      //----------------------------------------------------------------------
      session.remove();
   }
   
   public void test_referenceSharingWithinArray()
      throws Exception
   {
      StatelessSessionHome home = 
         (StatelessSessionHome)PortableRemoteObject.narrow(
                    getInitialContext().lookup(StatelessSessionHome.JNDI_NAME),
                    StatelessSessionHome.class);
      StatelessSession session = home.create();
      //----------------------------------------------------------------------
      int n = 100;
      Object[] original = new Object[n];
      for (int i = 0; i < n; i++)
         original[i] = new Boo("t" + i, "boo array test");
      Object[] echoedBack =
         session.testReferenceSharingWithinArray(original);
      assertEquals(2 * n, echoedBack.length);
      for (int i = 0; i < n; i++)
      {
         assertEquals(original[i], echoedBack[i]);
         assertEquals(original[i], echoedBack[i + n]);
         assertSame(echoedBack[i], echoedBack[i + n]);
      }
      //----------------------------------------------------------------------
      session.remove();
   }
   
   public void test_referenceSharingWithinCollection()
      throws Exception
   {
      StatelessSessionHome home = 
         (StatelessSessionHome)PortableRemoteObject.narrow(
                    getInitialContext().lookup(StatelessSessionHome.JNDI_NAME),
                    StatelessSessionHome.class);
      StatelessSession session = home.create();
      //----------------------------------------------------------------------
      java.util.Collection original = new java.util.ArrayList();
      int n = 10;
      for (int i = 0; i < n; i++)
         original.add(new Foo(100 + i, "foo collection test"));
      java.util.Collection echoedBack =
         session.testReferenceSharingWithinCollection(original);
      assertEquals(2 * n, echoedBack.size());
      java.util.ArrayList originalList = (java.util.ArrayList)original;
      java.util.ArrayList echoedList = (java.util.ArrayList)echoedBack;
      for (int i = 0; i < n; i++)
      {
         assertEquals(originalList.get(i), echoedList.get(i));
         assertEquals(originalList.get(i), echoedList.get(i + n));
         assertSame(echoedList.get(i), echoedList.get(i + n));
      }
      //----------------------------------------------------------------------
      session.remove();
   }

   public void test_bigVector()
      throws Exception
   {
      StatelessSessionHome home = 
         (StatelessSessionHome)PortableRemoteObject.narrow(
                    getInitialContext().lookup(StatelessSessionHome.JNDI_NAME),
                    StatelessSessionHome.class);
      StatelessSession session = home.create();
      //----------------------------------------------------------------------
      Foo[] original = new Foo[256];
      for (int i = 0; i < original.length; i++)
         original[i] = new Foo(100 + i, "foo vector test");
      java.util.Vector v = session.valueArrayToVector(original);
      Foo[] echoedBack = session.vectorToValueArray(v);
      assertEquals(original.length, echoedBack.length);
      for (int i = 0; i < echoedBack.length; i++)
         assertEquals(Util.echoFoo(Util.echoFoo(original[i])),
                      echoedBack[i]);
      //----------------------------------------------------------------------
      session.remove();
   }

   public void test_CorbaObject()
      throws Exception
   {
      StatelessSessionHome home = 
         (StatelessSessionHome)PortableRemoteObject.narrow(
                    getInitialContext().lookup(StatelessSessionHome.JNDI_NAME),
                    StatelessSessionHome.class);
      StatelessSession session = home.create();
      //----------------------------------------------------------------------
      final ORB orb = ORB.init(new String[0], System.getProperties());
      POA poa = POAHelper.narrow(orb.resolve_initial_references("RootPOA"));
      IdlInterfaceServant servant = new IdlInterfaceServant();
      org.omg.CORBA.Object original = servant._this_object(orb);
      poa.the_POAManager().activate();
      new Thread(
         new Runnable() {
            public void run() {
               orb.run();
            }
         }, 
         "ORB thread"
      ).start(); 
      org.omg.CORBA.Object echoedBack = session.testCorbaObject(original);
      assertEquals(orb.object_to_string(original), 
                   orb.object_to_string(echoedBack));
      //----------------------------------------------------------------------
      session.remove();
   }
   
   public void test_IdlInterface()
      throws Exception
   {
      StatelessSessionHome home = 
         (StatelessSessionHome)PortableRemoteObject.narrow(
                    getInitialContext().lookup(StatelessSessionHome.JNDI_NAME),
                    StatelessSessionHome.class);
      StatelessSession session = home.create();
      //----------------------------------------------------------------------
      final ORB orb = ORB.init(new String[0], System.getProperties());
      POA poa = POAHelper.narrow(orb.resolve_initial_references("RootPOA"));
      IdlInterfaceServant servant = new IdlInterfaceServant();
      org.omg.CORBA.Object obj = servant._this_object(orb);
      poa.the_POAManager().activate();
      new Thread(
         new Runnable() {
            public void run() {
               orb.run();
            }
         }, 
         "ORB thread"
      ).start(); 
      IdlInterface original = IdlInterfaceHelper.narrow(obj);
      IdlInterface echoedBack = session.testIdlInterface(original);
      assertEquals(orb.object_to_string(original), 
                   orb.object_to_string(echoedBack));
      
      //----------------------------------------------------------------------
      session.remove();
   }
   
   public static Test suite() throws Exception
   {
      return getDeploySetup(ParameterPassingStressTestCase.class, "iiop.jar");
   }

}
