/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cts.test;


import java.io.ByteArrayInputStream;
import java.io.ObjectInputStream;
import java.io.ByteArrayOutputStream;
import java.io.ObjectOutputStream;

import java.util.Collection;
import java.util.Iterator;
import java.util.Properties;

import javax.ejb.Handle;
import javax.ejb.HomeHandle;
import javax.ejb.EJBMetaData;

import javax.naming.InitialContext;

import javax.rmi.PortableRemoteObject;

import javax.transaction.UserTransaction;

import org.jboss.test.cts.jms.ContainerMBox;
import org.jboss.test.cts.interfaces.CtsBmpHome;
import org.jboss.test.cts.interfaces.CtsBmp;
import org.jboss.test.cts.interfaces.UserTransactionTester;
import org.jboss.test.cts.interfaces.StatelessSession;
import org.jboss.test.cts.keys.AccountPK;

import junit.framework.Test;

import org.jboss.test.JBossTestCase;


/**
 *  Class BmpTest
 *
 *  @author Author: kimptoc 
 *  @version $Revision: 1.4.4.2 $
 */


public class BmpUnitTestCase
   extends JBossTestCase
{
   private ContainerMBox mbx = null; 
   public static final String BEAN_NAME = "GuysName";
   public static final String BEAN_OTHER_NAME = "OtherGuysName";
   public static final String BEAN_PK_007 = "007";

   /**
    * Constructor BmpTest
    *
    * @param name
    *
    */
   public BmpUnitTestCase(String name)
   {
      super(name);
   }

   /**
    *  Return the bean home interface.
    */
   private CtsBmpHome getHome()
      throws Exception
   {
      return (CtsBmpHome)getInitialContext().lookup("ejbcts/BMPBean");
   }

   /**
    *  Create a bean instance.
    */
   private CtsBmp doEjbCreate(AccountPK pk, String name)
      throws Exception
   {
      return getHome().create(pk, name);
   }

   /**
    * Method testEjbCreate
    * EJB 1.1 [8.3.1] p. 89
    * An entity bean's home interface can define zero or more create(...)
    * methods. 
    *
    * @throws Exception
    *
    */
   public void testEjbCreate()
      throws Exception
   {
      getLog().debug(
         "**************************************************************");
      getLog().debug("     testEjbCreate()");

      CtsBmp bean = null;

      try {
         getLog().debug("create bean, name=" + BEAN_NAME);

         bean = doEjbCreate(new AccountPK(BEAN_PK_007), BEAN_NAME);
      } catch (Exception ex) {
         getLog().error("Error in bmptest", ex);
         fail("testEjbCreate has failed!");
      }

      assertEquals(BEAN_NAME, bean.getPersonsName());

      getLog().debug(
         "**************************************************************");
   }

  /**
    * Method testEjbFinder
    * EJB 1.1 [8.3.2] p. 90
    * An entity bean's home interface defines one or more finder methods,
    * one for each way to find and entity object or collection of entity objects
    * within the home.
    *
    * Test stategy: Create a bean. Use the bean that has been previously
    *               created, and call the finder method.  Make sure that
    *               a result set is returned and that the bean returned
    *               has the same name associated with it as the bean that
    *               was previously created.
    * 
    * @throws Exception
    *
    */
   public void testEjbFinder()
      throws Exception
   {
      getLog().debug(
         "**************************************************************");
      getLog().debug("     testEjbFinder()");

      CtsBmp bean = null;

      try {
         CtsBmpHome home = getHome();

         // First create a bean instance to find
         getLog().debug("Create bean, name=" + BEAN_NAME);
         doEjbCreate(new AccountPK(BEAN_PK_007), BEAN_NAME);

         getLog().debug("Find bean, name=" + BEAN_NAME);

         Collection clct = home.findByPersonsName(BEAN_NAME);
         getLog().debug("Verify result set not empty");
         assertTrue(!clct.isEmpty());
         getLog().debug("OK");
         getLog().debug("Bean result set:");
         for(Iterator itr=clct.iterator(); itr.hasNext();)
	 {
             bean = (CtsBmp)itr.next();
             getLog().debug("Name from Bean=" + bean.getPersonsName());
             getLog().debug("Verify bean name equals: " + BEAN_NAME);
             assertTrue(bean.getPersonsName().trim().equals(BEAN_NAME));
             getLog().debug("OK");              
	 }
      } catch (Exception ex) {
         getLog().error("Error in bmptest", ex);
         fail("testEjbFinder has failed!");
      }

      getLog().debug(
         "**************************************************************");
   }

   /**
    * Method testEjbRemove
    * EJB 1.1 [8.3.3] p. 90
    * 
    * Test Strategy:
    * 1) Create a bean to remove.
    * 2) Attempt a simple remove using the remote interface.
    * 3) Create a bean to remove.
    * 4) Attempt a simple remove using the home interface and primary key.
    * 5) Create a bean to remove.
    * 6) Try to remove the instance using its handle.
    * 7) Try to access the instance. This should result in a
    *    java.rmi.NoSuchObjectException
    *
    * @throws Exception
    */
   public void testEjbRemove()
      throws Exception
   {
      getLog().debug(
         "**************************************************************");
      getLog().debug("     testEjbRemove()");

      CtsBmp bean = null;

      try {
         CtsBmpHome home = getHome();
         AccountPK pk = new AccountPK(BEAN_PK_007);

         getLog().debug("Create a bean...");
         bean = doEjbCreate(pk, BEAN_NAME);
         getLog().debug("OK");

         getLog().debug("Delete with bean.remove()...");
         bean.remove();
         getLog().debug("OK");

         getLog().debug("Recreate the bean...");
         bean = doEjbCreate(pk, BEAN_NAME);
         getLog().debug("OK");

         getLog().debug("Remove the bean using primary key...");
         home.remove(pk);
         getLog().debug("OK");

         getLog().debug("Reconstitute the bean...");
         bean = doEjbCreate(pk, BEAN_NAME);
         getLog().debug("OK");

         getLog().debug("Get Handle object...");
         Handle hn = bean.getHandle( );
         getLog().debug("OK");

         getLog().debug("Remove the bean using the handle...");
         home.remove(hn);
         getLog().debug("OK");

         getLog().debug("Bean remove, try to use.. " +
                          "Should get 'java.rmi.NoSuchObjectException'..." );
         try {
	     bean.getPersonsName();
         } catch(java.rmi.NoSuchObjectException nsoex) {
	     getLog().debug("OK");
         } catch(Exception ex) {
	     fail("Got Exception: expecting NoSuchObjectException" + ex.toString()  );
         }
      } catch (Exception ex) {
         getLog().error("Error in bmptest", ex);
         fail("testEjbRemove has failed!");
      }

      getLog().debug(
         "**************************************************************");
   }

   /**
    * Method testEjbLifeCycle
    * EJB 1.1 [8.4] p. 92
    * 
    * A client can get a reference to an existing entity objects
    * remote interface in any of the following ways:
    * - Receive the reference as a parameter in a method call.
    * - Find the entity object using a finder method defined in the EB home i/f.
    * - Obtain the reference from the entity objects' handle.
    *
    * @throws Exception
    *
    */
   public void testEjbLifeCycle()
   {
      getLog().debug(
         "**************************************************************");
      getLog().debug("     testEjbLifeCycle()");

      CtsBmp bean = null;

      try {
         CtsBmpHome home = getHome();
         AccountPK pk = new AccountPK(BEAN_PK_007);

         getLog().debug("Create a bean...");
         doEjbCreate(pk, BEAN_NAME);
         getLog().debug("OK");

         getLog().debug("Use a finder method to retrieve the bean...");
         bean = home.findByPrimaryKey( pk );
         getLog().debug("OK");

         getLog().debug("Assert it is the same bean as passed to a method..." );
         // Send to a method as a reference, make sure it is usable by the method
         assertTrue( this.gotRefOkay(bean, BEAN_NAME) );
         getLog().debug("OK");

         // Execute a business method
         getLog().debug("Calling setter as a business method...");
         bean.setPersonsName(BEAN_OTHER_NAME);
         getLog().debug("OK");

         // Get the home interface
         getLog().debug("Get the HOME interface...");
         home = (CtsBmpHome)bean.getEJBHome();
         getLog().debug("OK");

         // Get the primary key
         getLog().debug("Get the bean's Primary Key...");
         pk = (AccountPK)bean.getPrimaryKey();
         getLog().debug("OK");

         getLog().debug("Get the bean's handle...");
         Handle hn = bean.getHandle();
         getLog().debug("OK");

         // Remove
         getLog().debug("Remove the bean...");
         bean.remove();
         getLog().debug("OK");
      } catch (Exception ex) {
         getLog().error("Error in bmptest", ex);
         fail("testEjbCreate has failed!");
      }

      getLog().debug(
         "**************************************************************");
   }

   /**
    * Method testPrimaryKeyObjectIdentity
    * EJB 1.1 [8.5] p. 92-93
    * 
    * Every entity object has a unique identity within its home.   If
    * two entity objects have the same home and the same primary key
    * they are considered identitcal.
    *
    * getPrimaryKey() always returns the same value when called one the
    * same entity object.
    * 
    * A client can test whether two entity object references refer to the 
    * same entity object by using the isIdentical(EBJObject) method. 
    * Alternatively, if a client obtains two entity object references from
    * the same home, it can determin if they refer to the same entity by comparing 
    * their primary keys using the 'equals' method.
    * 
    * @throws Exception
    *
    */
   public void testPrimaryKeyObjectIdentity()
   {
      getLog().debug(
          "**************************************************************");
      getLog().debug("     testPrimaryKeyObjectIdentity()");

      CtsBmp bean = null;
      CtsBmp anotherBean = null;
      CtsBmp differentBean = null;

      try {
         CtsBmpHome home = getHome();
         AccountPK pk = new AccountPK(BEAN_PK_007);

         getLog().debug("Create a bean...");
         bean = doEjbCreate(pk, BEAN_NAME);
         getLog().debug("OK");

         getLog().debug("Now query based on the 'PersonsName': " +
                          BEAN_NAME + "...");
         Collection clct = home.findByPersonsName(BEAN_NAME);
         getLog().debug("OK");

         getLog().debug("Verify result set not empty...");
         assertTrue(!clct.isEmpty());
         getLog().debug("OK");

         getLog().debug("Bean result set:");
         for (Iterator itr=clct.iterator(); itr.hasNext();)
         {
            anotherBean = (CtsBmp)itr.next();
            getLog().debug("Use 'isIdentical()' to compare beans");
            assertTrue(anotherBean.isIdentical(bean));
            getLog().debug( "beans match..OK" );
         }

         getLog().debug("Make a bean that doesn't match..");
         AccountPK anotherPK = new AccountPK("123");
         differentBean = doEjbCreate(anotherPK, "SomeOtherGuy");
         getLog().debug("OK");

         getLog().debug("Use 'isIdentical()' to verify different beans...");
         assertTrue(!differentBean.isIdentical(bean));
         getLog().debug("OK...beans are different!");

         getLog().debug("Test the Primary Keys...");
         AccountPK beansPK = (AccountPK)bean.getPrimaryKey();
         AccountPK anotherBeansPK = (AccountPK)anotherBean.getPrimaryKey();
         assertTrue(beansPK.equals(anotherBeansPK));
         getLog().debug("OK...they're the same");

         getLog().debug("Compare different keys...");
         assertTrue(!beansPK.equals(anotherPK));
         getLog().debug("OK...they're different");

         getLog().debug(
           "**************************************************************");

      } catch(Exception ex) {
         getLog().error("Error in bmptest", ex);
         fail("Caught an unknown exception: " + ex.toString() );
      }
   }

   /**
    * Method testEjbRemoteIF
    * EJB 1.1 [8.6] p. 93-94
    *
    * The javax.ejb.EJBObject I/F defines the methods that allow the client
    * to perform the following:
    * - Obtain the home interface for the entity object
    * - Remove the entity object
    * - Obtain the entity object's handle
    * - Obtain the entity object's primary key 
    * 
    * @throws Exception
    *
    */
   public void testEjbRemoteIF()
   {
     getLog().debug(
         "**************************************************************");
     getLog().debug("     testEjbRemoteIF ()");

      CtsBmp bean = null;

      try {
         CtsBmpHome home = getHome();
         AccountPK pk = new AccountPK(BEAN_PK_007);

         getLog().debug("Create a bean...");
         bean = doEjbCreate(pk, BEAN_NAME);
         getLog().debug("OK");

         getLog().debug("Obtain the HOME interface...");
         home = (CtsBmpHome)bean.getEJBHome();
         assertTrue(home != null);
         getLog().debug("OK");

         getLog().debug("Obtain the HANDLE...");
         Handle han = bean.getHandle();
         assertTrue(han != null);
         getLog().debug("OK");

         getLog().debug("Obtain the primary key...");
         pk = (AccountPK)bean.getPrimaryKey();
         assertTrue(pk != null);
         getLog().debug("OK");

         getLog().debug("Remove the entity bean");
         bean.remove();
         getLog().debug("OK");
      } catch(Exception ex) {
         getLog().error("Error in bmptest", ex);
         fail("Caught an unknown exception" + ex.toString());
      }

      getLog().debug(
         "**************************************************************");
   }

   /**
    * Method testEntityHandle
    * EJB 1.1 [8.7] p. 93-94
    *
    * - Client can get handle to remote interface
    * - Use javax.rmi.PortableRemoteObject.narrow(...) to convert the 
    *   result of the getEJBObject().
    * - An entity handle is typically implemented to be usable over a 
    *   long period of time it must be usable at least across a server
    *   restart.
    * 
    * @throws Exception
    *
    */
   public void testEntityHandle()
   {
     getLog().debug(
         "**************************************************************");
     getLog().debug("     testEntityHandle()"); 

      CtsBmp bean = null;

      try {
         CtsBmpHome home = getHome();
         AccountPK pk = new AccountPK(BEAN_PK_007);

         getLog().debug("Create a bean...");
         bean = doEjbCreate(pk, BEAN_NAME);
         getLog().debug("OK");

         getLog().debug("Get a Handle reference and serialize it...");
         Handle beanHandle = bean.getHandle();
         ByteArrayOutputStream out = new ByteArrayOutputStream();
         ObjectOutputStream sOut = new ObjectOutputStream(out);
         sOut.writeObject(beanHandle);
         sOut.flush();
         byte[] bytes = out.toByteArray();
         getLog().debug("OK");
 
         getLog().debug("Unserialize bean handle...");
         ByteArrayInputStream in = new ByteArrayInputStream(bytes);
         ObjectInputStream sIn = new ObjectInputStream(in);
         beanHandle = (Handle)sIn.readObject();
         getLog().debug("OK");

         getLog().debug("Use PortableRemoteObject to narrow result...");
         bean = (CtsBmp)PortableRemoteObject.narrow(beanHandle.getEJBObject(),
                                                    CtsBmp.class);
         getLog().debug("OK");

         getLog().debug("Check that new reference works...");
         assertTrue(bean.getPersonsName().trim().equals(BEAN_NAME));
         getLog().debug("OK");
      } catch(Exception ex) {
         getLog().error("Error in bmptest", ex);
         fail("Caught an unknown exeption: " + ex.toString());
      }

     getLog().debug(
         "**************************************************************");

   }

   /** Test of handle that is unmarshalled in a environment where
    * new InitialContext() will not work. This must use the
    * @throws Exception
    */
   public void testSessionHandleNoDefaultJNDI()
         throws Exception
   {
      getLog().debug("+++ testSessionHandleNoDefaultJNDI()");

      /* We have to establish the JNDI env by creating a InitialContext with
      the org.jboss.naming.NamingContextFactory. Normally this would be done
      during the home lookup and session creation.
      */
      Properties homeProps = new Properties();
      homeProps.setProperty("java.naming.factory.initial", "org.jboss.naming.NamingContextFactory");
      InitialContext ic = new InitialContext(homeProps);
      CtsBmpHome home = (CtsBmpHome) ic.lookup("ejbcts/BMPBean");
      AccountPK pk = new AccountPK(BEAN_PK_007);
      CtsBmp bean = doEjbCreate(pk, BEAN_NAME);
      Handle beanHandle = bean.getHandle();
      ByteArrayOutputStream out = new ByteArrayOutputStream();
      ObjectOutputStream oos = new ObjectOutputStream(out);
      oos.writeObject(beanHandle);
      oos.flush();
      byte[] bytes = out.toByteArray();

      Properties sysProps = System.getProperties();
      Properties newProps = new Properties(sysProps);
      newProps.setProperty("java.naming.factory.initial", "badFactory");
      newProps.setProperty("java.naming.provider.url", "jnp://badhost:12345");
      System.setProperties(newProps);
      try
      {
         getLog().debug("Unserialize bean handle...");
         ByteArrayInputStream in = new ByteArrayInputStream(bytes);
         ObjectInputStream ois = new ObjectInputStream(in);
         beanHandle = (Handle) ois.readObject();
         bean = (CtsBmp) beanHandle.getEJBObject();
         String name = bean.getPersonsName();
         getLog().debug("getPersonsName: "+name);
      }
      finally
      {
         System.setProperties(sysProps);
      }
   }

   /**
    *  Method testProbeContainerCallbacks
    */
   public void testProbeContainerCallbacks()
   {
      getLog().debug(
         "**************************************************************");
      getLog().debug("     testProbeContainerCallbacks()");

      CtsBmp bean = null;

      try {
         CtsBmpHome home = getHome();
         AccountPK pk = new AccountPK(BEAN_PK_007);

         mbx.clearMessages();

         getLog().debug("Create a bean...");
         bean = doEjbCreate(pk, BEAN_NAME);
         getLog().debug("OK");

         getLog().debug("Check for set entity context, create " +
                          "and post create messages...");
         Thread.sleep(2500);
         // OSH: We cannot be sure that the context will be set:
         // If the container elects a pooled instance to use for the
         // new object, setEntityContext() may have been called before
         // we cleared the message box.
         //assertTrue(mbx.messageReceived(ContainerMBox.SET_ENTITY_CONTEXT_MSG));
         assertTrue("Expected to receive notification of EJB_CREATE_MSG",mbx.messageReceived(ContainerMBox.EJB_CREATE_MSG));
         assertTrue("Expected to receive notification of EJB_POST_CREATE_MSG",mbx.messageReceived(ContainerMBox.EJB_POST_CREATE_MSG));
         getLog().debug("OK");

         // Execute a business method
         getLog().debug("Calling setter as a business method...");
         bean.setPersonsName(BEAN_OTHER_NAME);
         getLog().debug("OK");

         // Remove
         getLog().debug("Remove the bean...");
         bean.remove();
         Thread.sleep(3000);
         assertTrue("Expected to receive notification of EJB_STORE_MSG",mbx.messageReceived(ContainerMBox.EJB_STORE_MSG));
         assertTrue("Expected to receive notification of EJB_REMOVE_MSG",mbx.messageReceived(ContainerMBox.EJB_REMOVE_MSG));
         getLog().debug("OK");
      } catch (Exception ex) {
         getLog().error("Error in bmptest", ex);
         fail("testEjbCreate has failed!");
      }

      getLog().debug(
         "**************************************************************");
   }

   /**
    * Method testContainerObjects
    * EJB 1.1 [9.3] p. 127-129
    * Container must implement:
    *  - Entity EJBHome class
    *  - Entity EJBObject class
    *  - Handle class
    *  - HomeHandle class
    *  - Meta-data class
    */
   public void testContainerObjects()
   {
      getLog().debug(
         "**************************************************************");
      getLog().debug("     testContainerObjects()");

      CtsBmp bean = null;

      try {
         CtsBmpHome home = getHome();
         AccountPK pk = new AccountPK(BEAN_PK_007);

         mbx.clearMessages();

         getLog().debug("Create a bean...");
         bean = doEjbCreate(pk, BEAN_NAME);
         getLog().debug("OK");

         getLog().debug("Get HomeHandle..." );
         HomeHandle homeHan = home.getHomeHandle();
         assertTrue(homeHan != null);   
         getLog().debug("OK");

         getLog().debug("Get another home from the HomeHandle...");
         CtsBmpHome anotherHome = (CtsBmpHome)homeHan.getEJBHome();
         assertTrue(anotherHome != null);
         getLog().debug("OK");

         getLog().debug("Get the Meta-data object...");
         EJBMetaData md = anotherHome.getEJBMetaData();
         assertTrue(md != null);
         getLog().debug("OK");

         getLog().debug("Probe the Meta-data object:");
         String homeInterface = md.getHomeInterfaceClass().getName();
         String primaryKey = md.getPrimaryKeyClass().getName();
         String remoteInterface = md.getRemoteInterfaceClass().getName();
         getLog().debug("  Home Interface  : " + homeInterface);
         getLog().debug("  PrimaryKey      : " + primaryKey);
         getLog().debug("  Remote Interface: " + remoteInterface);
         assertTrue(homeInterface.equals("org.jboss.test.cts.interfaces.CtsBmpHome"));
         assertTrue(primaryKey.equals("org.jboss.test.cts.keys.AccountPK"));
         assertTrue(remoteInterface.equals("org.jboss.test.cts.interfaces.CtsBmp"));
         getLog().debug("Meta-data OK");

         getLog().debug("Check isSession()==false ...");
         assertTrue(!md.isSession());
         getLog().debug("OK");

         getLog().debug("Check isStatelessSession()==false ...");
         assertTrue(!md.isStatelessSession());
         getLog().debug("OK");

         getLog().debug("Test EJBHome.remove(PrimaryKey)");
         anotherHome.remove(pk);
         getLog().debug("OK");

      } catch (Exception ex) {
         getLog().error("Error in bmptest", ex);
         fail("testEjbCreate has failed!");
      }

      getLog().debug(
         "**************************************************************");
   }

   /**
    *  Do the UserTransaction tests.
    */
   public void testUserTransaction()
      throws Exception
   {
      getLog().debug(
         "**************************************************************");
      getLog().debug("     testUserTransaction()");
 
      CtsBmpHome home = getHome();
      UserTransaction ut;
 
      getLog().debug("Obtain UserTransaction...");
      Object o = new InitialContext().lookup("UserTransaction");
      ut = (UserTransaction)PortableRemoteObject.narrow(o, UserTransaction.class);
      assertTrue(ut != null);
      getLog().debug("OK");
 
      getLog().debug("Do UserTransaction tests...");
      UserTransactionTester utt = new UserTransactionTester(home, ut);
      assertTrue(ut != null);
      assertTrue(utt.runAllTests());
      getLog().debug("Ok");
 
      getLog().debug(
         "**************************************************************");
   }
  
   // Used to test passing a Entity bean as a parameter.
   // OSH: ??? This just calls a method on the bean ???
   private boolean gotRefOkay(CtsBmp bean, String expectedName)
   {
      boolean retVal = false;

      try {
	 getLog().debug(expectedName + "==" + bean.getPersonsName()+"?");
         retVal =  (bean.getPersonsName().equals(expectedName));
      } catch(Exception ex) {
         getLog().debug("Unknown Exception : " + ex.toString() );
      } 

      return retVal;
   }
    
    //do the mbox setup/teardown for each test separately
   protected void setUp()
      throws Exception
   {
      getLog().debug("Build Container MBX for BMP");
      mbx = new ContainerMBox();  

      getLog().debug("Initialize to empty BMP table.");
      CtsBmpHome home = getHome();
      Collection clct = home.findAll();
      if (clct.size() != 0) {
         getLog().debug("Removing " + clct.size() + " old beans.");
         for (Iterator itr=clct.iterator(); itr.hasNext();) {
            CtsBmp bean = (CtsBmp)itr.next();
            bean.remove();
	 }
         getLog().debug("Removal done.");              
      }
   }

   protected void tearDown()
      throws Exception
   {
      try {
         mbx.close();
      } catch (Exception ignoredBecauseProblemsWillBeHighlightedAsTestFailures)
      {}
   }

    //deploy the cts.jar once for the suite.
   public static Test suite() throws Exception
   {
      return getDeploySetup(BmpUnitTestCase.class, "cts.jar");
   }


}
