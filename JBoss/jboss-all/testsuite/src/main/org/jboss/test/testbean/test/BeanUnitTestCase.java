/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/

package org.jboss.test.testbean.test;

import java.rmi.*;


import javax.naming.Context;
import javax.naming.InitialContext;
import javax.ejb.DuplicateKeyException;
import javax.ejb.Handle;
import javax.ejb.EJBMetaData;
import javax.ejb.EJBHome;
import javax.ejb.HomeHandle;

import java.util.Date;
import java.util.Properties;
import java.util.Collection;
import java.util.Iterator;
import java.util.Enumeration;
import java.util.Arrays;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.jboss.test.testbean.interfaces.StatelessSessionHome;
import org.jboss.test.testbean.interfaces.StatelessSession;
import org.jboss.test.testbean.interfaces.StatefulSessionHome;
import org.jboss.test.testbean.interfaces.StatefulSession;
import org.jboss.test.testbean.interfaces.EntityBMPHome;
import org.jboss.test.testbean.interfaces.EntityBMP;
import org.jboss.test.testbean.interfaces.EnterpriseEntityHome;
import org.jboss.test.testbean.interfaces.EnterpriseEntity;
import org.jboss.test.testbean.interfaces.EntityPKHome;
import org.jboss.test.testbean.interfaces.EntityPK;
import org.jboss.test.testbean.interfaces.BusinessMethodException;
import org.jboss.test.testbean.interfaces.AComplexPK;
import org.jboss.test.testbean.interfaces.TxSessionHome;
import org.jboss.test.testbean.interfaces.TxSession;
import org.jboss.test.testbean.interfaces.BMTStatefulHome;
import org.jboss.test.testbean.interfaces.BMTStateful;
import org.jboss.test.testbean.interfaces.BMTStatelessHome;
import org.jboss.test.testbean.interfaces.BMTStateless;
import org.jboss.test.testbean2.interfaces.AllTypes;
import org.jboss.test.testbean2.interfaces.AllTypesHome;
import org.jboss.test.testbean2.interfaces.MyObject;

import org.jboss.test.bmp.interfaces.BMPHelperSession;
import org.jboss.test.bmp.interfaces.BMPHelperSessionHome;
import org.jboss.test.bmp.interfaces.SimpleBMP;
import org.jboss.test.bmp.interfaces.SimpleBMPHome;

import org.jboss.test.JBossTestCase;
import org.jboss.invocation.MarshalledValue;

/**
* Sample client for the jboss container.
*
* @author <a href="mailto:marc.fleury@ejboss.org">Marc Fleury</a>
* @author <a href="mailto:hugo@hugopinto.com">Hugo Pinto</a>
* @version $Id: BeanUnitTestCase.java,v 1.11.2.2 2002/12/12 03:07:03 jboynes Exp $
*/
public class BeanUnitTestCase
extends JBossTestCase
{
   static boolean deployed = false;
   static int test = 0;
   static Date startDate = new Date();

   protected final String namingFactory =
   System.getProperty(Context.INITIAL_CONTEXT_FACTORY);

   protected final String providerURL =
   System.getProperty(Context.PROVIDER_URL);

   public BeanUnitTestCase(String name) {
      super(name);
   }


   public void testRealBMP()
   throws Exception
   {

      getLog().debug("");
      getLog().debug("Test Real BMP (load/passivation/...");
      getLog().debug("===================================");
      getLog().debug("");

      BMPHelperSessionHome sessionHome = (BMPHelperSessionHome)new InitialContext ().lookup ("bmp.BMPHelperSession");
      BMPHelperSession session = sessionHome.create ();

      getLog().debug ("looking up table:");
      if (!session.existsSimpleBeanTable ())
      {
         getLog().debug ("table does not exist.");
         getLog().debug ("create it...");
         session.createSimpleBeanTable();
         getLog().debug ("done.");
      }

      SimpleBMPHome home = (SimpleBMPHome)new InitialContext ().lookup ("bmp.SimpleBMP");

      getLog().debug(++test+"- "+"create bean1: 1, Daniel");
      SimpleBMP b1 = home.create (1, "Daniel");
      getLog().debug ("getName (): "+b1.getName ());

      getLog().debug(++test+"- "+"create bean2: 2, Robert");
      b1 = home.create (2, "Robert");
      getLog().debug ("getName (): "+b1.getName ());

      try
      {
         getLog().debug(++test+"- trying to create one with same primkey: 1, Patrick");
         b1 = home.create (1, "Patrick");
         fail("Was able to create duplicate SimpleBMP");
      }
      catch (Exception _e)
      {
         getLog().debug (_e.toString ());
      }

      getLog().debug(++test+"- create some more dummys:");
      for (int i = 0; i < 50; ++i)
         home.create (i + 3, ("Dummy "+i));

      getLog().debug(++test+"- trying to find Robert again");
      b1 = home.findByPrimaryKey (new Integer (2));
      getLog().debug ("getName (): "+b1.getName ());

      try
      {
         getLog().debug(++test+"- trying to find an not existing bean");
         b1 = home.findByPrimaryKey (new Integer (0));
         assertTrue("findByPrimaryKey(0) should fail", b1 == null);
      }
      catch (Exception _e)
      {
         getLog().debug (_e.toString ());
      }

      getLog().debug(++test+"- rename Daniel to Maria: 1, Daniel");
      b1 = home.findByPrimaryKey (new Integer (1));
      getLog().debug ("name old: " + b1.getName ());
      b1.setName ("Maria");
      assertTrue("getName == Maria", "Maria".equals(b1.getName ()));

      getLog().debug(++test+"- find all beans:");
      Iterator it = home.findAll ().iterator ();
      while (it.hasNext ())
      {
         getLog().debug ("found:"+((SimpleBMP)it.next ()).getName ());
      }

      getLog().debug(++test+"- Now trying from within the Session bean (to be able to rollback):");
      getLog().debug (session.doTest ());

      getLog().debug(++test+"- get name after rollback ");
      getLog().debug (session.doTestAfterRollback ());

      getLog().debug(++test+"- removing all beans");
      it = home.findAll ().iterator ();
      while (it.hasNext ())
         ((SimpleBMP)it.next ()).remove ();

      getLog().debug ("drop table...");
      session.dropSimpleBeanTable();
      getLog().debug ("done.");
   }


   public void testStatelessBean()
   throws Exception
   {

      getLog().debug(++test+"- "+"Trying the context...");

      Context ctx = new InitialContext();
      getLog().debug("OK");

      ///*
      getLog().debug("");
      getLog().debug("Test Stateless Bean");
      getLog().debug("===================");
      getLog().debug("");
      getLog().debug(++test+"- "+"Looking up the home nextgen.StatelessSession...");
      StatelessSessionHome  statelessSessionHome =
      (StatelessSessionHome) ctx.lookup("nextgen.StatelessSession");
      if (statelessSessionHome!= null ) getLog().debug("ok");
         getLog().debug(++test+"- "+"Calling create on StatelessSessionHome...");
      StatelessSession statelessSession =
      statelessSessionHome.create();
      assertTrue("statelessSessionHome.create() != null", statelessSession != null);
      getLog().debug("ok");

      getLog().debug(++test+"- "+"Calling getEJBHome() on StatelessSession...");
      assertTrue("statelessSession.getEJBHome() != null", statelessSession.getEJBHome() != null);
      getLog().debug("ok");

      getLog().debug(++test+"- "+"Calling Business Method A on StatelessSession... ");
      statelessSession.callBusinessMethodA();
      getLog().debug("ok");
      getLog().debug(++test+"- "+"Calling Business Method B on StatelessSession... ");
      getLog().debug(statelessSession.callBusinessMethodB());
      getLog().debug(++test+"- "+"Calling Business Method B(String) on StatelessSession... ");
      getLog().debug(statelessSession.callBusinessMethodB("of wisdom"));
      getLog().debug(++test+"- "+"Calling Business Method C on StatelessSession... ");
      getLog().debug(statelessSession.callBusinessMethodC());
      getLog().debug(++test+"- "+"Calling Business Method D on StatelessSession... ");
      try
      {
         statelessSession.callBusinessMethodD();
         fail("callBusinessMethodD, no exception was thrown");
      }
      catch (BusinessMethodException e)
      {
         getLog().debug("Caught BusinessMethodException OK");
      }
      getLog().debug(++test+"- "+"Calling Business Method E (getEJBObject) on StatelessSession... ");
      getLog().debug(statelessSession.callBusinessMethodE());

      getLog().debug(++test+"- "+"Calling testClassLoading on StatelessSession... ");
      statelessSession.testClassLoading();
      getLog().debug("OK");

      getLog().debug("***Testing the various local Object class calls");
      getLog().debug(++test+"- "+"toString ... " + statelessSession.toString());

      getLog().debug(++test+"- "+"hashCode ... " + statelessSession.hashCode());

      getLog().debug(++test+"- "+"equals (same object) ... " + statelessSession.equals(statelessSession));

      getLog().debug(++test+"- "+"equals (another object) (true under same home)... " + statelessSession.equals(statelessSessionHome.create()));

      getLog().debug("***Testing the various local EJBObject class calls");
      getLog().debug(++test+"- "+"Get Primary Key ... " + statelessSession.getPrimaryKey());

      getLog().debug(++test+"- "+"Get Handle ... ");
      Handle statelessHandle = statelessSession.getHandle();
      assertTrue("statelessHandle != null", statelessHandle != null);
      getLog().debug("OK");
      getLog().debug(++test+"- "+"Serialize handle and deserialize..");
      MarshalledObject mo = new MarshalledObject(statelessHandle);
      Handle handle2 = (Handle) mo.get();
      StatelessSession statelessSession2 = (StatelessSession) handle2.getEJBObject();
      assertTrue("statelessSession2 != null", statelessSession2 != null);
      getLog().debug("OK");
      getLog().debug(++test+"- "+"Calling businessMethodB on it...");
      getLog().debug(statelessSession2.callBusinessMethodB());
      getLog().debug(++test+"- "+"They should be identical..."+statelessSession.isIdentical(statelessSession2));
      getLog().debug("***Testing the various local EJBHome class calls");
      getLog().debug(++test+"- "+"Getting the metaData...");
      EJBMetaData statelessMetaData = statelessSessionHome.getEJBMetaData();
      assertTrue("statelessMetaData != null", statelessMetaData != null);
      getLog().debug("OK");
      getLog().debug(++test+"- "+"Is stateless Session? "+statelessMetaData.isStatelessSession());
      getLog().debug(++test+"- "+"The remote class is "+statelessMetaData.getRemoteInterfaceClass());

      getLog().debug("");
      getLog().debug(++test+"- "+"Calling StatelessSession.remove()...");
      statelessSession.remove();
      getLog().debug("ok");
   }

   public void testStatefulBean()
   throws Exception
   {
      Context ctx = new InitialContext();

      getLog().debug("");
      getLog().debug("Test Stateful Bean");
      getLog().debug("==================");
      getLog().debug("");
      getLog().debug(++test+"- "+"Looking up the home nextgen.StatefulSession...");
      StatefulSessionHome  statefulSessionHome =
      (StatefulSessionHome) ctx.lookup("nextgen.StatefulSession");
      assertTrue("statefulSessionHome!= null", statefulSessionHome!= null);
      getLog().debug("ok");
      getLog().debug(++test+"- "+"Calling create on StatefulSessionHome with name Marc...");
      StatefulSession statefulSession =
      statefulSessionHome.create("Marc");
      assertTrue("statefulSession != null", statefulSession != null);
      getLog().debug("ok");
      getLog().debug(++test+"- "+"Calling getEJBHome() on StatefulSession...");
      assertTrue("statefulSession.getEJBHome() != null", statefulSession.getEJBHome() != null);
      getLog().debug("ok");
      getLog().debug(++test+"- "+"Calling Business Method A on StatefulSession... ");
      getLog().debug(statefulSession.callBusinessMethodA());
      getLog().debug(++test+"- "+"Calling Business Method A (state) on StatefulSession... ");
      getLog().debug(statefulSession.callBusinessMethodA());
      getLog().debug(++test+"- "+"Calling Business Method B (EJBObject) on StatefulSession... ");
      getLog().debug(statefulSession.callBusinessMethodB());

      getLog().debug(++test+"- "+"Calling Business Method B(String) on StatefulSession... ");
      getLog().debug(statefulSession.callBusinessMethodB("of wisdom"));


      getLog().debug("***Testing the various local Object class calls");
      getLog().debug(++test+"- "+"toString ... ");
      getLog().debug(statefulSession.toString());
      getLog().debug(++test+"- "+"hashCode ... " +
                     statefulSession.hashCode());

      getLog().debug(++test+"- "+"equals (same object) ... " +
                     statefulSession.equals(statefulSession));

      getLog().debug(++test+"- "+"equals (another object) (false under same home)... " +
                     statefulSession.equals(statefulSessionHome.create("marc4")));

      getLog().debug("***Testing the various local EJBObject class calls");
      getLog().debug(++test+"- "+"Get Primary Key ... " +
                     statefulSession.getPrimaryKey());

      getLog().debug(++test+"- "+"Get Handle ... ");
      Handle statefulHandle = statefulSession.getHandle();
      assertTrue("statefulHandle != null", statefulHandle != null);
      getLog().debug("OK");
      getLog().debug(++test+"- "+"Serialize handle and deserialize....");
      MarshalledObject mo2 = new MarshalledObject(statefulHandle);
      Handle statefulHandle2 = (Handle) mo2.get();
      StatefulSession statefulSession2 = (StatefulSession) statefulHandle2.getEJBObject();
      assertTrue("statefulSession2 != null", statefulSession2 != null);
      getLog().debug("OK");
      getLog().debug(++test+"- "+"Calling businessMethodB on it..." +
                     statefulSession2.callBusinessMethodB());

      getLog().debug(++test+"- "+"They should be identical..." +
                     statefulSession.isIdentical(statefulSession2));

      getLog().debug(++test+"- "+"Calling StatefulSession.remove()...");
      statefulSession.remove();
      getLog().debug("ok");
      getLog().debug(++test+"- "+"Calling StatefulHome.remove(Handle) (this should fail)...");
      try
      {
         statefulSessionHome.remove(statefulSession2.getHandle());
         fail("statefulSessionHome.remove did not fail");
      }
      catch (Exception e)
      {
         getLog().debug("not found OK");
      }
      getLog().debug(++test+"- "+"Creating a 3rd bean and calling it...");
      StatefulSession ss3 = statefulSessionHome.create("marc3");
      getLog().debug(ss3.callBusinessMethodA());
      getLog().debug(++test+"- "+"Calling StatefulSession.remove(Handle) on a third bean...");
      Handle statefulHandle3 = ss3.getHandle();
      statefulSessionHome.remove(statefulHandle3);
      getLog().debug("OK");
      getLog().debug(++test+"- "+"I should not be able to remove it directly...");
      try {
         ss3.remove();
         fail("ss3.remove() did not fail");
      } catch (Exception e) {
         getLog().debug("OK");
      }

      getLog().debug(++test+"- "+"Creating a 4th bean using create<METHOD> and calling it...");
      StatefulSession ss4 = statefulSessionHome.createMETHOD("marc4", "address");
      getLog().debug(ss4.callBusinessMethodA());
      getLog().debug(++test+"- "+"Calling StatefulSession.remove(Handle) on a fourth bean...");
      Handle statefulHandle4 = ss4.getHandle();
      statefulSessionHome.remove(statefulHandle4);
   }

   public void testEntityBeanCMP()
   throws Exception
   {
      Context ctx = new InitialContext();

      getLog().debug("testEntityBeanCMP");
      getLog().debug(++test+"- "+"Looking up the home nextgen.EnterpriseEntity...ok");

      EnterpriseEntityHome enterpriseEntityHome = (EnterpriseEntityHome) ctx.lookup("nextgen.EnterpriseEntity");
      getLog().debug(++test+"- "+"Calling find on EnterpriseEntityHome with name Marc...");
      EnterpriseEntity enterpriseEntity = null;
      try
      {
         enterpriseEntity = enterpriseEntityHome.findByPrimaryKey("Marc");
      }
      catch (Exception e)
      {
         getLog().debug("findByPrimaryKey(Marc) failed", e);
      }
      if (enterpriseEntity == null)
      {
         getLog().debug("not found OK");
         getLog().debug(++test+"- "+"Calling create on EnterpriseEntityHome with name Marc...");
         enterpriseEntity = enterpriseEntityHome.create("Marc");
      }

      if (enterpriseEntity != null)
         getLog().debug("ok, enterpriseEntity"+enterpriseEntity+", hashCode="+enterpriseEntity.hashCode());

      getLog().debug(++test+"- "+"Calling for duplicate create and DuplicateKeyException...");
      try
      {
         Object e = enterpriseEntityHome.create("Marc");
         getLog().debug("I Really should not make it here, e="+e+", hashCode="+e.hashCode());
         throw new Exception ("DuplicateKey not seen");
      }
      catch (DuplicateKeyException dke)
      {
         getLog().debug("DuplicateKeyException ok");
      }

      getLog().debug(++test+"- "+"Calling getEJBHome() on EntityCMP...");
      assertTrue("enterpriseEntity.getEJBHome() != null", enterpriseEntity.getEJBHome() != null);
      getLog().debug("ok");

      getLog().debug(++test+"- "+"Getting a new reference with findByPK...");
      EnterpriseEntity enterpriseEntity2 = null;
      try {
         enterpriseEntity2 = enterpriseEntityHome.findByPrimaryKey("Marc");
      }
      catch (Exception re) {
         getLog().debug("Exception: ", re);
      }
      assertTrue("enterpriseEntity2 != null", enterpriseEntity2 != null);
      getLog().debug("ok");
      getLog().debug(++test+"- "+"Calling Business Method A on enterpriseEntity... ");
      getLog().debug(enterpriseEntity.callBusinessMethodA());

      getLog().debug(++test+"- "+"Calling Business Method A (again to ejbLoad if TypeC) on enterpriseEntity... ");
      getLog().debug(enterpriseEntity.callBusinessMethodA());

      getLog().debug(++test+"- "+"Calling Business Method B (EJBObject from entity) on enterpriseEntity...");
      getLog().debug(enterpriseEntity.callBusinessMethodB());

      getLog().debug(++test+"- "+"Calling Business Method B(String) on EnterpriseEntity... ");
      getLog().debug(enterpriseEntity.callBusinessMethodB("of wisdom"));

      getLog().debug(++test+"- "+"Calling getOtherField (non pk) on enterpriseEntity...");
      getLog().debug("value: "+enterpriseEntity.getOtherField());

      getLog().debug(++test+"- "+"Calling setOtherField(4) on enterpriseEntity...");
      enterpriseEntity.setOtherField(4);
      getLog().debug("OK");

      getLog().debug(++test+"- "+"Calling getOtherField() on enterpriseEntity (should be 4)...");
      int value = enterpriseEntity.getOtherField();
      assertTrue("enterpriseEntity.getOtherField() == 4", value == 4);
      getLog().debug("value is "+value+", OK");

      getLog().debug("***Testing the various local Object class calls");
      getLog().debug(++test+"- "+"toString ... " + enterpriseEntity);

      getLog().debug(++test+"- "+"hashCode ... " + enterpriseEntity.hashCode());

      getLog().debug(++test+"- "+"equals (same object) ... " +
                     enterpriseEntity.equals(enterpriseEntity));

      getLog().debug(++test+"- "+"equals (another object) (true for this case)... " +
                     enterpriseEntity.equals(enterpriseEntity2));

      getLog().debug("***Testing the various local EJBObject class calls");
      getLog().debug(++test+"- "+"Get Primary Key ... " +
                     enterpriseEntity.getPrimaryKey());

      getLog().debug(++test+"- "+"Get Handle ... ");
      Handle entityHandle = enterpriseEntity.getHandle();
      assertTrue("entityHandle != null", entityHandle != null);
      getLog().debug("OK");
      getLog().debug(++test+"- "+"Serialize handle and deserialize....");
      MarshalledObject mo3 = new MarshalledObject(entityHandle);
      Handle entityHandle3 = (Handle) mo3.get();
      EnterpriseEntity enterpriseEntity3 = (EnterpriseEntity) entityHandle3.getEJBObject();
      if (enterpriseEntity3 != null) getLog().debug("OK");
         getLog().debug(++test+"- "+"Calling businessMethodA on it...");
      getLog().debug(enterpriseEntity3.callBusinessMethodB());
      getLog().debug(++test+"- "+"They should be identical..."+enterpriseEntity.isIdentical(enterpriseEntity3));
      getLog().debug(++test+"- "+"Calling entityHome.remove(Handle)...");
      enterpriseEntityHome.remove(enterpriseEntity3.getHandle());
      getLog().debug("OK");

      getLog().debug(++test+"- "+"Calling enterpriseEntity.remove() (should fail)...");
      try {
         enterpriseEntity.remove();
         fail("enterpriseEntity.remove() did not fail");
      }
      catch (Exception e) {
         getLog().debug("OK");
      }

      getLog().debug(++test+"- "+"Calling EnterpriseEntity.create() for marc6...");
      EnterpriseEntity marc6 = enterpriseEntityHome.create("marc6");
      getLog().debug("ok");

      getLog().debug(++test+"- "+"Calling method createEntity on enterpriseEntity... ");
      EnterpriseEntity marc2 = marc6.createEntity("marc2");
      getLog().debug("OK");


      getLog().debug(++test+"- "+"removing by PK on home (marc2)...");
      enterpriseEntityHome.remove(marc2.getPrimaryKey());
      getLog().debug("ok");

      getLog().debug(++test+"- "+"Calling enterpriseEntity.remove()  (marc6)...");
      marc6.remove();
      getLog().debug("ok");

      getLog().debug(++test+"- "+"Calling EnterpriseEntity.create<METHOD>() for marc7...");
      EnterpriseEntity marc7 = enterpriseEntityHome.createMETHOD("marc7");
      getLog().debug("ok");

      getLog().debug(++test+"- "+"Calling enterpriseEntity.remove()  (marc7)...");
      marc7.remove();
      getLog().debug("ok");



      getLog().debug("");
      getLog().debug("");
      getLog().debug("");
   }

   public void testEntityBeanBMP()
   throws Exception
   {
      Context ctx = new InitialContext();

      getLog().debug("");
      getLog().debug("");
      getLog().debug("Test Entity Bean BMP");
      getLog().debug("====================");
      getLog().debug("");


      getLog().debug(++test+"- "+"Looking up home for nextgen.EntityBMP...");
      EntityBMPHome bmpHome = (EntityBMPHome) ctx.lookup("nextgen.EntityBMP");
      assertTrue("bmpHome != null", bmpHome != null);
      getLog().debug("ok");

      // the current test will always return

      getLog().debug(++test+"- "+"Calling create on the home...");
      EntityBMP bmpBean = bmpHome.create("Marc");
      assertTrue("bmpBean != null", bmpBean != null);
      getLog().debug("ok");

      // Let's call a business method to see the flow of server calls

      getLog().debug(++test+"- "+"Calling getEJBHome() on EntityBMP...");
      assertTrue("bmpBean.getEJBHome() != null", bmpBean.getEJBHome() != null);
      getLog().debug("ok");

      getLog().debug(++test+"- "+"Calling business methodA on BMP bean...");
      getLog().debug(bmpBean.callBusinessMethodA());
      getLog().debug(++test+"- "+"Calling business methodB (B2B) on BMP bean and it says ");
      getLog().debug(bmpBean.callBusinessMethodB());


      getLog().debug(++test+"- "+"Calling Business Method B(String) on BMP... ");
      getLog().debug(bmpBean.callBusinessMethodB("of wisdom"));

      getLog().debug(++test+"- "+"calling remove() on BMP...");
      bmpBean.remove();

      getLog().debug(++test+"- "+"calling findCollectionKeys....");
      Collection pks = bmpHome.findCollectionKeys(3);
      Iterator pkIterator = pks.iterator();
      while (pkIterator.hasNext()) {
         EntityBMP currentBean = (EntityBMP)pkIterator.next();
         getLog().debug((String)currentBean.getPrimaryKey());
      }
      getLog().debug("ok");

      getLog().debug(++test+"- "+"calling findEnumeratedKeys....");
      Enumeration pksEnumeration = bmpHome.findEnumeratedKeys(3);
      while (pksEnumeration.hasMoreElements()) {
         EntityBMP currentBean = (EntityBMP)pksEnumeration.nextElement();
         getLog().debug((String)currentBean.getPrimaryKey());
      }
      getLog().debug("ok");

      getLog().debug(++test+"- "+"Calling create<METHOD> on the home...");
      bmpBean = bmpHome.createMETHOD("Marc2");
      assertTrue("bmpBean != null", bmpBean != null);
      getLog().debug("ok");

      getLog().debug(++test+"- "+"calling remove() on BMP...");
      bmpBean.remove();
   }

   public void testEntityBeanPK()
   throws Exception
   {
      Context ctx = new InitialContext();

      getLog().debug("");
      getLog().debug("");
      getLog().debug("Test Entity Bean PK");
      getLog().debug("====================");
      getLog().debug("");

      getLog().debug(++test+"- "+"Looking up home for nextgen.EntityPK...");
      EntityPKHome pkHome = (EntityPKHome) ctx.lookup("nextgen.EntityPK");
      assertTrue("pkHome != null", pkHome != null);
      getLog().debug("ok");

      getLog().debug(++test+"- "+"Calling find on the home...");
      EntityPK pkBean = null;

      // Let's try to find the instance
      try {

         pkBean =  pkHome.findByPrimaryKey(new AComplexPK(true, 10, 100, 1000.0, "Marc"));
      } catch (Exception e) {

         getLog().debug("not found");
         getLog().debug(++test+"- "+"Did not find the instance will create it...");
         pkBean = pkHome.create(true, 10,100, 1000.0, "Marc");
      }


      assertTrue("pkBean != null", pkBean != null);
      getLog().debug("ok");

      getLog().debug(++test+"- "+"Retrieving other field...");
      assertTrue("pkBean.getOtherField() == 0", pkBean.getOtherField() == 0);
      getLog().debug(++test+"- "+"Setting it to 4...");
      pkBean.setOtherField(4);
      getLog().debug("ok");

      getLog().debug(++test+"- "+"Findind it again ... ") ;

      // Now we should be able to find it
      pkBean = pkHome.findByPrimaryKey(new AComplexPK(true, 10, 100,1000.0, "Marc"));

      assertTrue("pkBean != null", pkBean != null);
      getLog().debug("ok");

      // check if the other field has been saved
      getLog().debug(++test+"- "+"Retrieving other field again, should be 4...");
      int newValue = pkBean.getOtherField();
      assertTrue("pkBean.getOtherField() == 4", newValue == 4);
      getLog().debug("4, ok");

      // Get a new EJBObject for this guy
      // Now we should be able to find it
      getLog().debug(++test+"- gettting a new reference ... ") ;
      EntityPK pkBean2 = pkHome.findByPrimaryKey(new AComplexPK(true, 10, 100,1000.0, "Marc"));
      assertTrue("findByPrimaryKey AComplexPK", pkBean2 != null);
      getLog().debug("ok");
      getLog().debug(++test+"- Retrieving other field again, should be 4...");
      int newValue2 = pkBean2.getOtherField();
      assertTrue("Retrieving other field again, should be 4...", newValue2 == 4);

      // Now remove it
      getLog().debug(++test+"- "+"Removing the bean...");
      pkBean.remove();
      try
      {
         getLog().debug(++test+"- "+"I should not find it...");
         pkBean = pkHome.findByPrimaryKey(new AComplexPK(true, 10, 100, 1000.0, "Marc"));
         assertTrue("findByPrimaryKey should fail", pkBean == null);
      }
      catch (Exception e)
      {
         getLog().debug("not found, OK");
      }

      getLog().debug(++test+"- "+"Call create<METHOD> on EntityPKHome...");
      pkBean = pkHome.createMETHOD(true, 10,100, 1000.0, "Marc");
      getLog().debug(++test+"- "+"Removing the bean...");
      pkBean.remove();
   }

   public void testTxSession()
   throws Exception
   {
      Context ctx = new InitialContext();

      getLog().debug("");
      getLog().debug("");
      getLog().debug("Test TxSession");
      getLog().debug("==============");
      getLog().debug("");

      getLog().debug(++test+"- "+"Looking up home for nextgen.TxSession...");
      TxSessionHome txHome = (TxSessionHome) ctx.lookup("nextgen.TxSession");
      if (txHome != null )getLog().debug("ok");

      getLog().debug(++test+"- "+"Calling create on the home...");
      TxSession txBean = null;

      // Let's try to create the instance
      try {

         txBean =  txHome.create();
      } catch (Exception e) {

         getLog().debug("Exception: ", e);
      }


      if (txBean!= null) getLog().debug("ok");

      getLog().debug(++test+"- "+"calling supports... ");
      getLog().debug(txBean.txSupports());

      getLog().debug(++test+"- "+"calling required... ");
      getLog().debug(txBean.txRequired());

      getLog().debug(++test+"- "+"calling requiresNew... ");
      getLog().debug(txBean.txRequiresNew());

      getLog().debug(++test+"- "+"calling not supported... ");
      getLog().debug(txBean.txNotSupported());

      getLog().debug(++test+"- "+"calling mandatory (should get an exception)...");
      try {
         getLog().debug(txBean.txMandatory());
      } catch (Exception e) {
         getLog().debug("got Exception, ok");
      }

      getLog().debug(++test+"- "+"calling requiredToSupports... ");
      getLog().debug(txBean.requiredToSupports());

      getLog().debug(++test+"- "+"calling requiredToNotSupported... ");
      getLog().debug(txBean.requiredToNotSupported());

      getLog().debug(++test+"- "+"calling requiredToRequiresNew... ");
      getLog().debug(txBean.requiredToRequiresNew());

      getLog().debug("ok");
   }

   public void testAllTypesBean()
   throws Exception
   {

      try {
      Context ctx = new InitialContext();

      getLog().debug("");
      getLog().debug("");
      getLog().debug("Test AllTypesBean");
      getLog().debug("=================");
      getLog().debug("");
      getLog().debug(++test+"- "+"Looking up the home AllTypes...");
      AllTypesHome allTypesHome = (AllTypesHome) ctx.lookup("AllTypes");
      if (allTypesHome!= null ) getLog().debug("ok");

      getLog().debug(++test+"- "+"Getting the home handle...");
      HomeHandle homeHandle = allTypesHome.getHomeHandle();
      getLog().debug("OK");

      getLog().debug(++test+"- "+"Getting the home back from the handle...");
      EJBHome aHome = homeHandle.getEJBHome();
      getLog().debug("OK");

      getLog().debug(++test+"- "+"Getting metadata from home...");
      EJBMetaData aMetaData = aHome.getEJBMetaData();
      getLog().debug("OK");

      getLog().debug(++test+"- "+"Getting home from metadata...");
      aHome = aMetaData.getEJBHome();
      getLog().debug("OK");

      getLog().debug(++test+"- "+"Calling findByPrimaryKey on AllTypesHome with name seb...");

      AllTypes allTypes = null;
      try {
         allTypes = allTypesHome.findByPrimaryKey("seb");
      }
      catch (Exception e) {getLog().debug(e.getMessage());}
      if (allTypes == null) {

         getLog().debug("not found OK");
         getLog().debug(++test+"- "+"Calling create on AllTypesHome with name seb...");
         allTypes = allTypesHome.create("seb");
      }

      if (allTypes != null) getLog().debug("ok");

      getLog().debug(++test+"- "+"Calling business method A an AllTypes (B2B with external ejb-ref)...");
      getLog().debug("OK, result is" + allTypes.callBusinessMethodA());

      getLog().debug("Getting all the fields");
      getLog().debug(++test+"- "+"boolean " + allTypes.getBoolean() + " Ok");
      getLog().debug(++test+"- "+"byte " + allTypes.getByte() + " Ok");
      getLog().debug(++test+"- "+"short " + allTypes.getShort() + " Ok");
      getLog().debug(++test+"- "+"int " + allTypes.getInt() + " Ok");
      getLog().debug(++test+"- "+"long " + allTypes.getLong() + " Ok");
      getLog().debug(++test+"- "+"float " + allTypes.getFloat() + " Ok");
      getLog().debug(++test+"- "+"double " + allTypes.getDouble() + " Ok");
      getLog().debug("No char test yet, bug in jdk");
      getLog().debug(++test+"- "+"String " + allTypes.getString() + " Ok");
      getLog().debug(++test+"- "+"Date " + allTypes.getDate() + " Ok");
      getLog().debug(++test+"- "+"Timestamp " + allTypes.getTimestamp() + " Ok");

      getLog().debug(++test+"- "+"MyObject ");
      MyObject obj = allTypes.getObject();
      getLog().debug("OK");

      getLog().debug(++test+"- "+"getting handle of stateful...");
      Handle sfHandle = allTypes.getStateful();
      getLog().debug("OK");

      getLog().debug(++test+"- "+"getting the bean back from the handle...");
      StatefulSession sfBean = (StatefulSession)sfHandle.getEJBObject();
      getLog().debug("OK");

      getLog().debug(++test+"- "+"comparing serialized handles...");
      assertTrue(Arrays.equals(new MarshalledValue(sfHandle).toByteArray(), new MarshalledValue(sfBean.getHandle()).toByteArray()));
      getLog().debug("OK");

      getLog().debug(++test+"- "+"calling business method A on stateful: ");
      getLog().debug("OK, result is " + sfBean.callBusinessMethodA());

      getLog().debug(++test+"- "+"adding the stateful bean as an object in AllTypes..");
      allTypes.addObjectToList(sfBean);
      getLog().debug("OK");

      getLog().debug(++test+"- "+"getting handle of stateless...");
      Handle slHandle = allTypes.getStateless();
      getLog().debug("OK");

      getLog().debug(++test+"- "+"getting the bean back from the handle...");
      StatelessSession slBean = (StatelessSession)slHandle.getEJBObject();
      getLog().debug("OK");

      getLog().debug(++test+"- "+"comparing serialized handles...");
      assertTrue(Arrays.equals(new MarshalledValue(slHandle).toByteArray(), new MarshalledValue(slBean.getHandle()).toByteArray()));
      getLog().debug("OK");

      getLog().debug(++test+"- "+"calling business method B on stateless: ");
      getLog().debug("OK, result is " + slBean.callBusinessMethodB());

      getLog().debug(++test+"- "+"adding the stateless bean as an object in AllTypes..");
      allTypes.addObjectToList(slBean);
      getLog().debug("OK");

      getLog().debug(++test+"- "+"getting handle of entity...");
      Handle eeHandle = allTypes.getEntity();
      getLog().debug("OK");

      getLog().debug(++test+"- "+"getting the bean back from the handle...");
      EnterpriseEntity eeBean = (EnterpriseEntity)eeHandle.getEJBObject();
      getLog().debug("OK");

      getLog().debug(++test+"- "+"comparing serialized handles...");
      assertTrue(Arrays.equals(new MarshalledValue(eeHandle).toByteArray(), new MarshalledValue(eeBean.getHandle()).toByteArray()));
      getLog().debug("OK");

      getLog().debug(++test+"- "+"calling business method A on stateless: ");
      getLog().debug("OK, result is" + eeBean.callBusinessMethodA());

      getLog().debug(++test+"- "+"adding the entity bean as an object in AllTypes..");
      allTypes.addObjectToList(eeBean);
      getLog().debug("OK");

      getLog().debug(++test+"- "+"Getting the list of objects back (should contain the 3 beans)...");
      Collection coll = allTypes.getObjectList();
      assertEquals(coll.size(), 3);
      getLog().debug("OK");
      getLog().debug(++test+"- "+"stateful bean ");
      assertTrue(coll.contains(sfBean));
      getLog().debug("OK");
      getLog().debug(++test+"- "+"stateless bean ");
      assertTrue(coll.contains(slBean));
      getLog().debug("OK");
      getLog().debug(++test+"- "+"entity bean ");
      assertTrue(coll.contains(eeBean));
      getLog().debug("OK");

      getLog().debug("Testing automatically generated finders");

      getLog().debug(++test+"- "+"findAll()..");
      coll = allTypesHome.findAll();
      assertTrue(coll.contains(allTypes));
      getLog().debug("OK");

      getLog().debug(++test+"- "+"findByPrimaryKey()...");
      AllTypes result = allTypesHome.findByPrimaryKey("seb");
      assertTrue(result.equals(allTypes));
      getLog().debug("OK");

      getLog().debug(++test+"- "+"findByABoolean()..");
      coll = allTypesHome.findByABoolean(allTypes.getBoolean());
      assertTrue(coll.contains(allTypes));
      getLog().debug("OK");

      getLog().debug(++test+"- "+"findByAByte()..");
      coll = allTypesHome.findByAByte(allTypes.getByte());
      assertTrue(coll.contains(allTypes));getLog().debug("OK");

      getLog().debug(++test+"- "+"findByAShort()..");
      coll = allTypesHome.findByAShort(allTypes.getShort());
      assertTrue(coll.contains(allTypes));getLog().debug("OK");

      getLog().debug(++test+"- "+"findByAnInt()..");
      coll = allTypesHome.findByAnInt(allTypes.getInt());
      assertTrue(coll.contains(allTypes));getLog().debug("OK");

      getLog().debug(++test+"- "+"findByALong()..");
      coll = allTypesHome.findByALong(allTypes.getLong());
      assertTrue(coll.contains(allTypes));getLog().debug("OK");

      getLog().debug(++test+"- "+"findByAFloat()..");
      coll = allTypesHome.findByAFloat(allTypes.getFloat());
      assertTrue(coll.contains(allTypes));getLog().debug("OK");

      getLog().debug(++test+"- "+"findByADouble()..");
      coll = allTypesHome.findByADouble(allTypes.getDouble());
      assertTrue(coll.contains(allTypes));getLog().debug("OK");

      getLog().debug("No Char test yet, bug in jdk");

      getLog().debug(++test+"- "+"findByAString()..");
      coll = allTypesHome.findByAString(allTypes.getString());
      assertTrue(coll.contains(allTypes));getLog().debug("OK");

      getLog().debug(++test+"- "+"findByADate()..");
      coll = allTypesHome.findByADate(allTypes.getDate());
      assertTrue(coll.contains(allTypes));getLog().debug("OK");

      getLog().debug(++test+"- "+"findByATimestamp()..");
      coll = allTypesHome.findByATimestamp(allTypes.getTimestamp());
      assertTrue(coll.contains(allTypes));getLog().debug("OK");

      getLog().debug(++test+"- "+"findByAnObject()..");
      coll = allTypesHome.findByAnObject(allTypes.getObject());
      assertTrue(coll.contains(allTypes));getLog().debug("OK");

      getLog().debug(++test+"- "+"findByStatefulSession()..");
      coll = allTypesHome.findByStatefulSession((StatefulSession)allTypes.getStateful().getEJBObject());
      getLog().debug("size="+coll.size());
      for (Iterator i = coll.iterator(); i.hasNext();) {
		  Object o = i.next();
		  getLog().debug("o="+o);
	  }
      assertTrue(coll.contains(allTypes));getLog().debug("OK");

      getLog().debug(++test+"- "+"findByStatelessSession()..");
      coll = allTypesHome.findByStatelessSession((StatelessSession)allTypes.getStateless().getEJBObject());
      assertTrue(coll.contains(allTypes));getLog().debug("OK");

      getLog().debug(++test+"- "+"findByEnterpriseEntity()..");
      coll = allTypesHome.findByEnterpriseEntity((EnterpriseEntity)allTypes.getEntity().getEJBObject());
      assertTrue(coll.contains(allTypes));getLog().debug("OK");

      getLog().debug("Testing finders defined in jaws.xml");

      getLog().debug(++test+"- "+"findByMinInt()..");
      coll = allTypesHome.findByMinInt(0);
      assertTrue(coll.contains(allTypes));getLog().debug("OK");

      getLog().debug(++test+"- "+"findByIntAndDouble()..");
      coll = allTypesHome.findByIntAndDouble(allTypes.getInt(), allTypes.getDouble());
      assertTrue(coll.contains(allTypes));getLog().debug("OK");

      }
      catch (Exception e) {getLog().debug("Exception: ", e); throw e;}
   }

   public void testBeanManagedTransactionDemarcation()
   throws Exception
   {
      Context ctx = new InitialContext();
      try{
         getLog().debug("");
         getLog().debug("");
         getLog().debug("Test Bean Managed Transaction Demarcation");
         getLog().debug("=========================================");
         getLog().debug("");
         ;

         getLog().debug(++test+"- "+"Looking up the home BMTStateful...");
         BMTStatefulHome bmtSFHome = (BMTStatefulHome) ctx.lookup("BMTStateful");

         if (bmtSFHome != null ) getLog().debug("ok");

         getLog().debug(++test+"- "+"Calling create on BMTStatefulHome...");

         BMTStateful bmtSF = bmtSFHome.create();

         getLog().debug(++test+"- "+"Calling create(anything) on BMTStatefulHome...");

         bmtSFHome.create("coca");
         getLog().debug("OK");
         getLog().debug(++test+"- "+"Can the bean access its UserTransaction");
         getLog().debug(bmtSF.txExists());
         getLog().debug(++test+"- "+"Testing commit on UserTransaction");
         getLog().debug(bmtSF.txCommit());
         getLog().debug(++test+"- "+"Testing rollback on UserTransaction");
         getLog().debug(bmtSF.txRollback());
         getLog().debug(++test+"- "+"Beginning a transaction...");
         getLog().debug(bmtSF.txBegin());
         getLog().debug(++test+"- "+"Committing the transaction in another call...");
         getLog().debug(bmtSF.txEnd());
         getLog().debug(++test+"- "+"Creating a table for real db w/ tx test...");
         bmtSF.createTable();
         getLog().debug("OK, field value is:");
         getLog().debug(bmtSF.getDbField());
         getLog().debug(++test+"- "+"Updating the field in a transaction, commit...");
         bmtSF.dbCommit();
         getLog().debug("OK, field value is:");
         getLog().debug(bmtSF.getDbField());
         getLog().debug(++test+"- "+"Updating the field in a transaction, rollback...");
         bmtSF.dbRollback();
         getLog().debug("OK, field value is:");
         getLog().debug(bmtSF.getDbField());
         getLog().debug(++test+"- "+"Now dropping the table...");
         bmtSF.dropTable();
         getLog().debug("OK");
         getLog().debug(++test+"- "+"Looking up the home BMTStateful...");
         BMTStatelessHome bmtSLHome = (BMTStatelessHome) ctx.lookup("BMTStateless");
         if (bmtSLHome != null ) getLog().debug("ok");
            getLog().debug(++test+"- "+"Calling create on BMTStatelessHome...");
         BMTStateless bmtSL = bmtSLHome.create();
         getLog().debug("OK");
         getLog().debug(++test+"- "+"Can the bean access its UserTransaction");
         getLog().debug(bmtSL.txExists());
         getLog().debug(++test+"- "+"Testing commit on UserTransaction");
         getLog().debug(bmtSL.txCommit());
         getLog().debug(++test+"- "+"Testing rollback on UserTransaction");
         getLog().debug(bmtSL.txRollback());
         getLog().debug(++test+"- "+"Beginning a transaction (container should throw an exception)...");
         try {
            getLog().debug(bmtSL.txBegin());
            fail("bmtSL.txBegin() did not fail");
         } catch (RemoteException e) {
            getLog().debug(" ... OK, exception message: "+ e.getMessage());
         }

         //*/
         getLog().debug("");
         getLog().debug("");
         getLog().debug("");
         getLog().debug("Test OK, "+test+" tests run, congratulations");

         Date finishDate = new Date();
         getLog().debug("Tests took "+(finishDate.getTime()-startDate.getTime())+" milliseconds");
      }
      catch (Exception e) {

         getLog().debug("exception: ", e);  throw e;}

   }


   public static Test suite() throws Exception
   {
      Test t1 = getDeploySetup(BeanUnitTestCase.class, "bmp.jar");
      Test t2 = getDeploySetup(t1, "testbean.jar");
      Test t3 = getDeploySetup(t2, "testbean2.jar");
      return t3;
   }

}
