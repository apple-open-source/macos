/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.cmp2.lob;

import java.io.File;
import java.io.FileInputStream;
import java.io.ByteArrayOutputStream;
import java.io.StringWriter;
import java.io.InputStream;

import java.util.Collection;
import java.util.Iterator;
import java.util.Enumeration;
import java.util.ArrayList;

import javax.rmi.PortableRemoteObject;
import javax.naming.InitialContext;

import net.sourceforge.junitejb.EJBTestCase;
import junit.framework.Test;

import org.jboss.logging.Logger;
import org.jboss.test.JBossTestCase;




/**
 * A test suite to check JBoss data mapping to/from Large Binary Objects (LOBs).
 *
 * @see net.sourceforge.junitejb.EJBTestCase.
 *
 * @version <tt>$Revision: 1.1.2.1 $</tt>
 * @author  <a href="mailto:steve@resolvesw.com">Steve Coy</a>.
 *
 */
public class LOBUnitTestCase extends EJBTestCase
{

   // Constants -----------------------------------------------------
   private static final String   LOB_HOME_CONTEXT        = "cmp2/lob/Lob";
   private static final Integer  LOB_PK0                 = new Integer(0);
   private static final Integer  LOB_PK1                 = new Integer(1);
   private static final Integer  LOB_PK2                 = new Integer(2);
   private static final Integer  LOB_PK3                 = new Integer(3);
   private static final Integer  LOB_PK4                 = new Integer(4);
   private static final String   SMALL_TEXT_FILE_PATH    = "data/style.xsl";
   private static final String   BIG_TEXT_FILE_PATH      = "data/page.html";
   private static final String   SMALL_BINARY_FILE_PATH  = "data/smallimage.png";
   private static final String   BIG_BINARY_FILE_PATH    = "data/image.png";
   
   // Attributes ----------------------------------------------------
   private LOBHome               mHome;
   private String                mSmallString;
   private String                mBigString;
   private byte[]                mSmallBinaryData;
   private byte[]                mBigBinaryData;

   // Static --------------------------------------------------------

   private static final Logger   msLog = Logger.getLogger(LOBUnitTestCase.class);

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
      msLog.info("testCreate1");
      LOB aLob = mHome.create(LOB_PK0);
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
      msLog.info("testNullLoad");
      LOB aLob = mHome.findByPrimaryKey(LOB_PK0);
      assertNull(aLob.getBigString());
      assertNull(aLob.getBinaryData());      
   }


   /**
    * Attempt to create a LOB entity with the small dataset.
    */
   public void testCreate1()
      throws Exception
   {
      msLog.info("testCreate1");
      LOB aLob = mHome.create(LOB_PK1);
      aLob.setBigString(mSmallString);
      aLob.setBinaryData(mSmallBinaryData);
   }

   
   /**
    * Verify the data set created by {@link #testCreate1}.
    */
   public void testLoad1()
      throws Exception
   {
      msLog.info("testLoad1");
      LOB aLob = mHome.findByPrimaryKey(LOB_PK1);
      assertEquals(mSmallString, aLob.getBigString());
      assertEquals(mSmallBinaryData, aLob.getBinaryData());
   }

   /**
    * Attempt to create a LOB entity with a large text object
    * and a small binary object.
    */
   public void testCreate2()
      throws Exception
   {
      msLog.info("testCreate2");
      LOB aLob = mHome.create(LOB_PK2);
      aLob.setBigString(mBigString);
      aLob.setBinaryData(mSmallBinaryData);
   }


   /**
    * Verify the data set created by {@link testCreate2}.
    */
   public void testLoad2()
      throws Exception
   {
      msLog.info("testLoad2");
      LOB aLob = mHome.findByPrimaryKey(LOB_PK2);
      assertEquals(mBigString, aLob.getBigString());
      assertEquals(mSmallBinaryData, aLob.getBinaryData());
   }

   /**
    * Attempt to create a LOB entity with the small text object
    * and a large binary object.
    */
   public void testCreate3()
      throws Exception
   {
      msLog.info("testCreate3");
      LOB aLob = mHome.create(LOB_PK3);
      aLob.setBigString(mSmallString);
      aLob.setBinaryData(mBigBinaryData);
   }


   /**
    * Verify the data set created by {@link testCreate3}.
    */
   public void testLoad3()
      throws Exception
   {
      msLog.info("testLoad3");
      LOB aLob = mHome.findByPrimaryKey(LOB_PK3);
      assertEquals(mSmallString, aLob.getBigString());
      assertEquals(mBigBinaryData, aLob.getBinaryData());
   }

   /**
    * Attempt to create a LOB entity with the large dataset.
    */
   public void testCreate4()
      throws Exception
   {
      msLog.info("testCreate4");
      LOB aLob = mHome.create(LOB_PK4);
      aLob.setBigString(mBigString);
      aLob.setBinaryData(mBigBinaryData);
   }


   /**
    * Verify the data set created by {@link testCreate4}.
    */
   public void testLoad4()
      throws Exception
   {
      msLog.info("testLoad4");
      LOB aLob = mHome.findByPrimaryKey(LOB_PK4);
      assertEquals(mBigString, aLob.getBigString());
      assertEquals(mBigBinaryData, aLob.getBinaryData());
   }

   /**
    * Attempt to load each entity in turn and verify that they contain the
    * text data with which they were created.
    */
   public void testTextLoad()
      throws Exception
   {
      msLog.info("testTextLoad");
      LOB aLob = mHome.findByPrimaryKey(LOB_PK1);
      assertEquals(mSmallString, aLob.getBigString());
      
      aLob = mHome.findByPrimaryKey(LOB_PK2);
      assertEquals(mBigString, aLob.getBigString());

      aLob = mHome.findByPrimaryKey(LOB_PK3);
      assertEquals(mSmallString, aLob.getBigString());

      aLob = mHome.findByPrimaryKey(LOB_PK4);
      assertEquals(mBigString, aLob.getBigString());
   }


   /**
    * Attempt to load each entity in turn and verify that they contain the
    * binary data with which they were created.
    */
   public void testBinaryLoad()
      throws Exception
   {
      msLog.info("testBinaryLoad");
      LOB aLob = mHome.findByPrimaryKey(LOB_PK1);
      assertEquals(mSmallBinaryData, aLob.getBinaryData());

      aLob = mHome.findByPrimaryKey(LOB_PK2);
      assertEquals(mSmallBinaryData, aLob.getBinaryData());

      aLob = mHome.findByPrimaryKey(LOB_PK3);
      assertEquals(mBigBinaryData, aLob.getBinaryData());

      aLob = mHome.findByPrimaryKey(LOB_PK4);
      assertEquals(mBigBinaryData, aLob.getBinaryData());
   }


   /**
    * Lookup the LOB home and cache it.
    * Load the test data.
    */
   public void setUpEJB()
     throws Exception
   {
      msLog.info("setupEJB");
      InitialContext initialContext = new InitialContext();
      Object home = initialContext.lookup(LOB_HOME_CONTEXT);
      mHome = (LOBHome)PortableRemoteObject.narrow(home, LOBHome.class);
      
      mSmallBinaryData = loadBinaryData(SMALL_BINARY_FILE_PATH);
      msLog.info("Loaded " + mSmallBinaryData.length + " bytes of binary data");

      mBigBinaryData = loadBinaryData(BIG_BINARY_FILE_PATH);
      msLog.info("Loaded " + mBigBinaryData.length + " bytes of binary data");
      
      mSmallString = loadTextData(SMALL_TEXT_FILE_PATH);
      msLog.info("Loaded " + mSmallString.length() + " characters of text");
      
      mBigString = loadTextData(BIG_TEXT_FILE_PATH);
      msLog.info("Loaded " + mBigString.length() + " characters of text");
   }


   /**
    * Remove data references so that they can be garbage collected if needed.
    */
   public void tearDownEJB()
      throws Exception
   {
      msLog.info("tearDownEJB");
      mSmallBinaryData = null;
      mBigBinaryData = null;
      mSmallString = null;
      mBigString = null;
      mHome = null;
   }

   // Protected -------------------------------------------------------

   static void assertEquals(byte[] expected, byte[] actual)
   {
      assertEquals(expected.length, actual.length);
      for (int i = 0; i < expected.length; ++i)
         assertEquals(expected[i], actual[i]);
   }
   
   // Private -------------------------------------------------------

   
   /**
    * Return the content of the input stream provided as a byte array.
    * @param   input stream
    * @return  content as a byte array
    */
   private byte[] loadBinaryData(String resourceName)
      throws java.io.IOException
   {
      ClassLoader classLoader = Thread.currentThread().getContextClassLoader();
      InputStream input = classLoader.getResourceAsStream(resourceName);
      try
      {
         ByteArrayOutputStream baos = new ByteArrayOutputStream();
         try
         {
            int byteRead;
            while ((byteRead = input.read()) != -1)
               baos.write(byteRead);
            return baos.toByteArray();
         }
         finally
         {
            baos.close();
         }
      }
      finally
      {
         input.close();
      }
   }
  
  
   /**
    * Return the content of the input stream provided as a String.
    * @param   input stream
    * @return  content as a string
    */
   private String loadTextData(String resourceName)
      throws java.io.IOException
   {
      ClassLoader classLoader = Thread.currentThread().getContextClassLoader();
      InputStream input = classLoader.getResourceAsStream(resourceName);
      try
      {
         StringWriter stringWriter = new StringWriter();
         try
         {
            int byteRead;
            while ((byteRead = input.read()) != -1)
               stringWriter.write(byteRead);
            return stringWriter.toString();
         }
         finally
         {
            stringWriter.close();
         }
      }
      finally
      {
         input.close();
      }
   }

}
