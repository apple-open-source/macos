/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.cmp2.lob;


import java.io.ByteArrayOutputStream;
import java.io.StringWriter;
import java.io.InputStream;
import java.util.Map;
import java.util.Set;
import java.util.List;

import javax.rmi.PortableRemoteObject;
import javax.naming.InitialContext;

import net.sourceforge.junitejb.EJBTestCase;
import junit.framework.Test;

import org.jboss.logging.Logger;
import org.jboss.test.JBossTestCase;


/**
 * A test suite to check JBoss data mapping to/from Large Binary Objects (LOBs).
 *
 * @see net.sourceforge.junitejb.EJBTestCase
 *
 * @version <tt>$Revision: 1.1.2.4 $</tt>
 * @author  <a href="mailto:steve@resolvesw.com">Steve Coy</a>.
 * @author  <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
 */
public class LOBUnitTestCase extends EJBTestCase
{
   private static final Integer LOB_PK0 = new Integer(0);
   private static final Integer LOB_PK1 = new Integer(1);
   private static final Integer LOB_PK2 = new Integer(2);
   private static final Integer LOB_PK3 = new Integer(3);
   private static final Integer LOB_PK4 = new Integer(4);
   private String SMALL_TEXT_FILE_PATH = "data/style.xsl";
   private String BIG_TEXT_FILE_PATH = "data/page.html";
   private String SMALL_BINARY_FILE_PATH = "data/smallimage.png";
   private String BIG_BINARY_FILE_PATH = "data/image.png";

   // Attributes ----------------------------------------------------
   private LOBHome lobHome;
   private FacadeHome facadeHome;
   private boolean resourcesLoaded;
   private String smallString;
   private String bigString;
   private byte[] smallBlob;
   private byte[] bigBlob;

   // Static --------------------------------------------------------

   private static final Logger log = Logger.getLogger(LOBUnitTestCase.class);

   public static Test suite()
      throws Exception
   {
      return JBossTestCase.getDeploySetup(LOBUnitTestCase.class, "cmp2-lob.jar");
   }

   // Constructors --------------------------------------------------

   public LOBUnitTestCase(String name)
      throws java.io.IOException
   {
      super(name);
   }

   // Public --------------------------------------------------------

   /**
    * Attempt to create a LOB entity with NULL attributes.
    */
   public void testCreate0()
      throws Exception
   {
      log.debug("testCreate1");
      LOB aLob = lobHome.create(LOB_PK0);
      aLob.setBigString(null);
      aLob.setBinaryData(null);
   }

   /**
    * Attempt to load the entity created above and ensure that we can recover
    * the null attributes.
    */
   public void testNullLoad()
      throws Exception
   {
      log.debug("testNullLoad");
      LOB aLob = lobHome.findByPrimaryKey(LOB_PK0);
      assertNull(aLob.getBigString());
      assertNull(aLob.getBinaryData());
   }

   /**
    * Attempt to create a LOB entity with the small dataset.
    */
   public void testCreate1()
      throws Exception
   {
      log.debug("testCreate1");
      LOB aLob = lobHome.create(LOB_PK1);
      aLob.setBigString(smallString);
      aLob.setBinaryData(smallBlob);
   }

   /**
    * Verify the data set created by {@link #testCreate1}.
    */
   public void testLoad1()
      throws Exception
   {
      log.debug("testLoad1");
      LOB aLob = lobHome.findByPrimaryKey(LOB_PK1);
      assertEquals(smallString, aLob.getBigString());
      assertEquals(smallBlob, aLob.getBinaryData());
   }

   /**
    * Attempt to create a LOB entity with a large text object
    * and a small binary object.
    */
   public void testCreate2()
      throws Exception
   {
      log.debug("testCreate2");
      LOB aLob = lobHome.create(LOB_PK2);
      aLob.setBigString(bigString);
      aLob.setBinaryData(smallBlob);
   }

   /**
    * Verify the data set created by {@link#testCreate2}.
    */
   public void testLoad2()
      throws Exception
   {
      log.debug("testLoad2");
      LOB aLob = lobHome.findByPrimaryKey(LOB_PK2);
      assertEquals(bigString, aLob.getBigString());
      assertEquals(smallBlob, aLob.getBinaryData());
   }

   /**
    * Attempt to create a LOB entity with the small text object
    * and a large binary object.
    */
   public void testCreate3()
      throws Exception
   {
      log.debug("testCreate3");
      LOB aLob = lobHome.create(LOB_PK3);
      aLob.setBigString(smallString);
      aLob.setBinaryData(bigBlob);
   }


   /**
    * Verify the data set created by {@link#testCreate3}.
    */
   public void testLoad3()
      throws Exception
   {
      log.debug("testLoad3");
      LOB aLob = lobHome.findByPrimaryKey(LOB_PK3);
      assertEquals(smallString, aLob.getBigString());
      assertEquals(bigBlob, aLob.getBinaryData());
   }

   /**
    * Attempt to create a LOB entity with the large dataset.
    */
   public void testCreate4()
      throws Exception
   {
      log.debug("testCreate4");
      LOB aLob = lobHome.create(LOB_PK4);
      aLob.setBigString(bigString);
      aLob.setBinaryData(bigBlob);
   }

   /**
    * Verify the data set created by {@link#testCreate4}.
    */
   public void testLoad4()
      throws Exception
   {
      log.debug("testLoad4");
      LOB aLob = lobHome.findByPrimaryKey(LOB_PK4);
      assertEquals(bigString, aLob.getBigString());
      assertEquals(bigBlob, aLob.getBinaryData());
   }

   /**
    * Attempt to load each entity in turn and verify that they contain the
    * text data with which they were created.
    */
   public void testTextLoad()
      throws Exception
   {
      log.debug("testTextLoad");
      LOB aLob = lobHome.findByPrimaryKey(LOB_PK1);
      assertEquals(smallString, aLob.getBigString());

      aLob = lobHome.findByPrimaryKey(LOB_PK2);
      assertEquals(bigString, aLob.getBigString());

      aLob = lobHome.findByPrimaryKey(LOB_PK3);
      assertEquals(smallString, aLob.getBigString());

      aLob = lobHome.findByPrimaryKey(LOB_PK4);
      assertEquals(bigString, aLob.getBigString());
   }

   /**
    * Attempt to load each entity in turn and verify that they contain the
    * binary data with which they were created.
    */
   public void testBinaryLoad()
      throws Exception
   {
      log.debug("testBinaryLoad");
      LOB aLob = lobHome.findByPrimaryKey(LOB_PK1);
      assertEquals(smallBlob, aLob.getBinaryData());

      aLob = lobHome.findByPrimaryKey(LOB_PK2);
      assertEquals(smallBlob, aLob.getBinaryData());

      aLob = lobHome.findByPrimaryKey(LOB_PK3);
      assertEquals(bigBlob, aLob.getBinaryData());

      aLob = lobHome.findByPrimaryKey(LOB_PK4);
      assertEquals(bigBlob, aLob.getBinaryData());
   }

   //
   // Map, Set, List as a CMP field types
   //

   public void testMapCMPField() throws Exception
   {
      log.debug("testMapCMPField> start");
      Facade facade = facadeHome.create();
      Integer id = new Integer(111);
      try
      {
         facade.createLOB(id);

         // populate the map
         Map oldMap = facade.getMapField(id);
         facade.addMapEntry(id, "key", "value");
         Map curMap = facade.getMapField(id);
         assertTrue("!oldMap.equals(curMap)", !oldMap.equals(curMap));

         // try to put the same values
         oldMap = curMap;
         facade.addMapEntry(id, "key", "value");
         curMap = facade.getMapField(id);
         assertTrue("oldMap.equals(curMap)", oldMap.equals(curMap));
      }
      finally
      {
         try { facade.removeLOB(id); } catch(Exception e) {}
      }
   }

   public void testSetCMPField() throws Exception
   {
      log.debug("testSetCMPField> start");
      Facade facade = facadeHome.create();
      Integer id = new Integer(111);
      try
      {
         facade.createLOB(id);

         // populate the set
         Set oldSet = facade.getSetField(id);
         facade.addSetElement(id, "value");
         Set curSet = facade.getSetField(id);
         assertTrue("!oldSet.equals(curSet)", !oldSet.equals(curSet));

         // try to put the same values
         oldSet = curSet;
         facade.addSetElement(id, "value");
         curSet = facade.getSetField(id);
         assertTrue("oldSet.equals(curSet)", oldSet.equals(curSet));
      }
      finally
      {
         try { facade.removeLOB(id); } catch(Exception e) {}
      }
   }

   public void testListCMPField() throws Exception
   {
      log.debug("testListCMPField> start");
      Facade facade = facadeHome.create();
      Integer id = new Integer(111);
      try
      {
         facade.createLOB(id);

         // populate the list
         List oldList = facade.getListField(id);
         facade.addListElement(id, "value");
         List curList = facade.getListField(id);
         assertTrue("!oldList.equals(curList)", !oldList.equals(curList));

         // try to put the same values
         oldList = curList;
         facade.addListElement(id, "value");
         curList = facade.getListField(id);
         assertTrue("curList.size() - oldList.size() == 1", curList.size() - oldList.size() == 1);
      }
      finally
      {
         try { facade.removeLOB(id); } catch(Exception e) {}
      }
   }

   public void testBinaryDataField() throws Exception
   {
      log.debug("testBinaryDataField> start");
      Facade facade = facadeHome.create();
      Integer id = new Integer(111);
      try
      {
         facade.createLOB(id);

         // populate the list
         facade.setBinaryData(id, new byte[]{1, 2, 3});
         assertTrue("facade.getBinaryDataElement(id, 1) == 2",
            facade.getBinaryDataElement(id, 1) == 2);

         facade.setBinaryDataElement(id, 1, (byte)5);
         assertTrue("facade.getBinaryDataElement(id, 1) == 5",
            facade.getBinaryDataElement(id, 1) == 5);
      }
      finally
      {
         try { facade.removeLOB(id); } catch(Exception e) {}
      }
   }

   public void testValueHolder() throws Exception
   {
      log.debug("testValueHolder> start");
      Facade facade = facadeHome.create();
      Integer id = new Integer(555);
      try
      {
         facade.createLOB(id);

         assertTrue("facade.getValueHolderValue(id) == null", facade.getValueHolderValue(id) == null);

         facade.setValueHolderValue(id, "Avoka");
         assertTrue("facade.getValueHolderValue(id).equals(\"Avoka\")", facade.getValueHolderValue(id).equals("Avoka"));
      }
      finally
      {
         try { facade.removeLOB(id); } catch(Exception e) {}
      }
   }

   public void testCleanGetValueHolder() throws Exception
   {
      log.debug("testCleanGetValueHolder> start");
      Facade facade = facadeHome.create();
      Integer id = new Integer(777);
      try
      {
         facade.createLOB(id);

         assertTrue("facade.getCleanGetValueHolderValue(id) == null", facade.getCleanGetValueHolderValue(id) == null);

         facade.setCleanGetValueHolderValue(id, "Avoka");
         assertTrue("facade.getCleanGetValueHolderValue(id).equals(\"Avoka\")",
            facade.getCleanGetValueHolderValue(id).equals("Avoka"));

         facade.modifyCleanGetValueHolderValue(id, "Ataka");
         assertTrue("facade.getCleanGetValueHolderValue(id).equals(\"Avoka\")",
            facade.getCleanGetValueHolderValue(id).equals("Avoka"));
      }
      finally
      {
         try { facade.removeLOB(id); } catch(Exception e) {}
      }
   }

   public void testStateFactoryValueHolder() throws Exception
   {
      log.debug("testStateFactoryValueHolder> start");
      Facade facade = facadeHome.create();
      Integer id = new Integer(777);
      try
      {
         facade.createLOB(id);

         assertTrue("facade.getStateFactoryValueHolderValue(id) == null",
            facade.getStateFactoryValueHolderValue(id) == null);

         facade.modifyStateFactoryValueHolderValue(id, "Avoka");
         assertTrue("facade.getStateFactoryValueHolderValue(id) == null",
            facade.getStateFactoryValueHolderValue(id) == null);

         facade.setStateFactoryValueHolderValue(id, "Avoka");
         assertTrue("facade.getStateFactoryValueHolderValue(id).equals(\"Avoka\")",
            facade.getStateFactoryValueHolderValue(id).equals("Avoka"));
      }
      finally
      {
         try { facade.removeLOB(id); } catch(Exception e) {}
      }
   }

   /**
    * Lookup the LOB lobHome and cache it.
    * Load the test data.
    */
   public void setUpEJB()
      throws Exception
   {
      log.debug("setupEJB");

      if(!resourcesLoaded)
      {
         InitialContext initialContext = new InitialContext();
         Object home = initialContext.lookup(LOBHome.LOB_HOME_CONTEXT);
         lobHome = (LOBHome)PortableRemoteObject.narrow(home, LOBHome.class);
         home = initialContext.lookup(FacadeHome.JNDI_NAME);
         facadeHome = (FacadeHome)PortableRemoteObject.narrow(home, FacadeHome.class);

         smallString = loadTextData(SMALL_TEXT_FILE_PATH);
         bigString = loadTextData(BIG_TEXT_FILE_PATH);
         smallBlob = loadBinaryData(SMALL_BINARY_FILE_PATH);
         bigBlob = loadBinaryData(BIG_BINARY_FILE_PATH);
         resourcesLoaded = true;
      }
   }

   /**
    * Remove data references so that they can be garbage collected if needed.
    */
   public void tearDownEJB()
      throws Exception
   {
      log.debug("tearDownEJB");
   }

   // Protected -------------------------------------------------------

   static void assertEquals(byte[] expected, byte[] actual)
   {
      assertEquals(expected.length, actual.length);
      for(int i = 0; i < expected.length; ++i)
         assertEquals(expected[i], actual[i]);
   }

   // Private -------------------------------------------------------

   /**
    * Return the content of the input stream provided as a byte array.
    * @param   resourceName  resource to read
    * @return  content as a byte array
    */
   private static final byte[] loadBinaryData(String resourceName)
   {
      ClassLoader classLoader = Thread.currentThread().getContextClassLoader();
      InputStream input = classLoader.getResourceAsStream(resourceName);
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      try
      {
         int byteRead;
         while((byteRead = input.read()) != -1)
            baos.write(byteRead);
         return baos.toByteArray();
      }
      catch(Exception e)
      {
         throw new IllegalStateException(e.getMessage());
      }
      finally
      {
         try
         {
            baos.close();
         }
         catch(Exception e)
         {
         }
         try
         {
            input.close();
         }
         catch(Exception e)
         {
         }
      }
   }


   /**
    * Return the content of the input stream provided as a String.
    * @param   resourceName resource to read
    * @return  content as a string
    */
   private static final String loadTextData(String resourceName)
   {
      ClassLoader classLoader = Thread.currentThread().getContextClassLoader();
      InputStream input = classLoader.getResourceAsStream(resourceName);
      StringWriter stringWriter = new StringWriter();
      try
      {
         int byteRead;
         while((byteRead = input.read()) != -1)
            stringWriter.write(byteRead);
         return stringWriter.toString();
      }
      catch(Exception e)
      {
         throw new IllegalStateException(e.getMessage());
      }
      finally
      {
         try
         {
            stringWriter.close();
         }
         catch(Exception e)
         {
         }
         try
         {
            input.close();
         }
         catch(Exception e)
         {
         }
      }
   }
}
