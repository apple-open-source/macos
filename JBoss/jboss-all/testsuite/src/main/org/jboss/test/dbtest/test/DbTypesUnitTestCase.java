/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.dbtest.test;

import java.rmi.*;
import java.util.Collection;

import java.util.Date;
import java.util.Enumeration;
import java.util.Iterator;
import java.util.Properties;
import javax.ejb.DuplicateKeyException;
import javax.ejb.EJBMetaData;
import javax.ejb.FinderException;
import javax.ejb.Handle;

import javax.naming.Context;
import javax.naming.InitialContext;

import junit.framework.*;

import org.jboss.test.dbtest.interfaces.AllTypes;
import org.jboss.test.dbtest.interfaces.AllTypesHome;
import org.jboss.test.dbtest.interfaces.MyObject;
import org.jboss.test.dbtest.interfaces.Record;
import org.jboss.test.dbtest.interfaces.RecordHome;

import org.jboss.test.JBossTestCase;

/**
 * Test case to try out all db types using cmp against DefaultDS
 */
public class DbTypesUnitTestCase
       extends JBossTestCase
{

   static boolean deployed = false;

   /**
    * Constructor for the DbTypesUnitTestCase object
    *
    * @param name  Description of Parameter
    */
   public DbTypesUnitTestCase(String name)
   {
      super(name);
   }




   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testAllTypesBean() throws Exception
   {
      int test = 0;

      Context ctx = new InitialContext();

      getLog().debug(++test + "- " + "Looking up the home AllTypes...");

      AllTypesHome allTypesHome;

      try
      {
         allTypesHome = (AllTypesHome)ctx.lookup("AllTypes");

         if (allTypesHome == null)
         {
            throw new Exception("abort");
         }
         getLog().debug("OK");

      }
      catch (Exception e)
      {
         getLog().debug("Could not lookup the context:  the beans are probably not deployed");
         getLog().debug("Check the server trace for details");
         log.debug("failed", e);
         throw new Exception();
      }

      getLog().debug(++test + "- " + "Calling findByPrimaryKey on AllTypesHome with name seb...");

      AllTypes allTypes = null;

      try
      {
         allTypes = allTypesHome.findByPrimaryKey("seb");
      }
      catch (Exception e)
      {
         getLog().debug(e.getMessage());
      }

      if (allTypes == null)
      {

         getLog().debug("not found OK");
         getLog().debug(++test + "- " + "Calling create on AllTypesHome with name seb...");
         allTypes = allTypesHome.create("seb");
      }

      if (allTypes != null)
      {
         getLog().debug("OK");
      }
      else
      {
         getLog().debug("Could not find or create the alltypes bean");
         getLog().debug("Check the server trace for details");

         throw new Exception();
      }

      getLog().debug("Getting all the fields");
      getLog().debug(++test + "- " + "boolean " + allTypes.getBoolean() + " OK");
      getLog().debug(++test + "- " + "byte " + allTypes.getByte() + " OK");
      getLog().debug(++test + "- " + "short " + allTypes.getShort() + " OK");
      getLog().debug(++test + "- " + "int " + allTypes.getInt() + " OK");
      getLog().debug(++test + "- " + "long " + allTypes.getLong() + " OK");
      getLog().debug(++test + "- " + "float " + allTypes.getFloat() + " OK");
      getLog().debug(++test + "- " + "double " + allTypes.getDouble() + " OK");
      getLog().debug("No char test yet, bug in jdk");
      getLog().debug(++test + "- " + "String " + allTypes.getString() + " OK");
      getLog().debug(++test + "- " + "Date " + allTypes.getDate() + " OK");
      getLog().debug(++test + "- " + "Time " + allTypes.getTime() + " OK");
      getLog().debug(++test + "- " + "Timestamp " + allTypes.getTimestamp() + " OK");

      getLog().debug(++test + "- " + "MyObject ");
      MyObject obj = allTypes.getObject();
      getLog().debug("OK");

      getLog().debug(++test + "- " + "Creating Record beans and adding them to the Collection in Alltypes..");
      RecordHome recordHome = (RecordHome)ctx.lookup("Record");

      Record[] record = new Record[3];
      for (int i = 0; i < 3; i++)
      {
         try
         {
            record[i] = recordHome.findByPrimaryKey("bill " + i);
         }
         catch (FinderException e)
         {
            record[i] = recordHome.create("bill " + i);
         }

         record[i].setAddress("SanFrancisco, CA 9411" + i);
         allTypes.addObjectToList(record[i]);
      }
      getLog().debug("OK");

      getLog().debug(++test + "- " + "Getting them back..");

      Collection collection = allTypes.getObjectList();
      boolean ok = true;

      for (int i = 0; i < 3; i++)
      {
         ok = ok && collection.contains(record[i]);
      }

      if (ok)
      {
         getLog().debug("OK");
      }
      else
      {
         getLog().debug("failed");
         throw new Exception("abort");
      }

      getLog().debug("All basic tests passed; Now testing min/max values.");
      getLog().debug("This is just for information, it's okay if some fail.");
      getLog().debug("Not all DBs have a column type that supports 8-byte numbers.");
      // NOTE: In order from most likely to fail to least likely to fail
      //       Oracle in particular demonstrates cascading failures and
      //       we don't want to miss that

      // Double
      try
      {
         allTypes.setDouble(Double.MIN_VALUE);
         double d;
         if ((d = allTypes.getDouble()) == Double.MIN_VALUE)
         {
            getLog().debug(++test + "- Double Min Value OK");
         }
         else
         {
            getLog().debug(++test + "- Double Min Value Different (" + d + " <> " + Double.MIN_VALUE + ")");
         }
      }
      catch (Exception e)
      {
         getLog().debug(++test + "- Double Min Value Failed");
      }
      try
      {
         allTypes.setDouble(Double.MAX_VALUE);
         double d;
         if ((d = allTypes.getDouble()) == Double.MAX_VALUE)
         {
            getLog().debug(++test + "- Double Max Value OK");
         }
         else
         {
            getLog().debug(++test + "- Double Max Value Different (" + d + " <> " + Double.MAX_VALUE + ")");
         }
      }
      catch (Exception e)
      {
         getLog().debug(++test + "- Double Max Value Failed");
      }
      // Float
      try
      {
         allTypes.setFloat(Float.MIN_VALUE);
         float f;
         if ((f = allTypes.getFloat()) == Float.MIN_VALUE)
         {
            getLog().debug(++test + "- Float Min Value OK");
         }
         else
         {
            getLog().debug(++test + "- Float Min Value Different (" + f + " <> " + Float.MIN_VALUE + ")");
         }
      }
      catch (Exception e)
      {
         getLog().debug(++test + "- Float Min Value Failed");
      }
      try
      {
         allTypes.setFloat(Float.MAX_VALUE);
         float f;
         if ((f = allTypes.getFloat()) == Float.MAX_VALUE)
         {
            getLog().debug(++test + "- Float Max Value OK");
         }
         else
         {
            getLog().debug(++test + "- Float Max Value Different (" + f + " <> " + Float.MAX_VALUE + ")");
         }
      }
      catch (Exception e)
      {
         getLog().debug(++test + "- Float Max Value Failed");
      }

      // Long
      try
      {
         allTypes.setLong(Long.MIN_VALUE);
         long l;
         if ((l = allTypes.getLong()) == Long.MIN_VALUE)
         {
            getLog().debug(++test + "- Long Min Value OK");
         }
         else
         {
            getLog().debug(++test + "- Long Min Value Different (" + l + " <> " + Long.MIN_VALUE + ")");
         }
      }
      catch (Exception e)
      {
         getLog().debug(++test + "- Long Min Value Failed");
      }
      try
      {
         allTypes.setLong(Long.MAX_VALUE);
         long l;
         if ((l = allTypes.getLong()) == Long.MAX_VALUE)
         {
            getLog().debug(++test + "- Long Max Value OK");
         }
         else
         {
            getLog().debug(++test + "- Long Max Value Different (" + l + " <> " + Long.MAX_VALUE + ")");
         }
      }
      catch (Exception e)
      {
         getLog().debug(++test + "- Long Max Value Failed");
      }
      // Short
      try
      {
         allTypes.setShort(Short.MIN_VALUE);
         short s;
         if ((s = allTypes.getShort()) == Short.MIN_VALUE)
         {
            getLog().debug(++test + "- Short Min Value OK");
         }
         else
         {
            getLog().debug(++test + "- Short Min Value Different (" + s + " <> " + Short.MIN_VALUE + ")");
         }
      }
      catch (Exception e)
      {
         getLog().debug(++test + "- Short Min Value Failed");
      }
      try
      {
         allTypes.setShort(Short.MAX_VALUE);
         short s;
         if ((s = allTypes.getShort()) == Short.MAX_VALUE)
         {
            getLog().debug(++test + "- Short Max Value OK");
         }
         else
         {
            getLog().debug(++test + "- Short Max Value Different (" + s + " <> " + Short.MAX_VALUE + ")");
         }
      }
      catch (Exception e)
      {
         getLog().debug(++test + "- Short Max Value Failed");
      }
      // Byte
      try
      {
         allTypes.setByte(Byte.MIN_VALUE);
         byte b;
         if ((b = allTypes.getByte()) == Byte.MIN_VALUE)
         {
            getLog().debug(++test + "- Byte Min Value OK");
         }
         else
         {
            getLog().debug(++test + "- Byte Min Value Different (" + b + " <> " + Byte.MIN_VALUE + ")");
         }
      }
      catch (Exception e)
      {
         getLog().debug(++test + "- Byte Min Value Failed");
      }
      try
      {
         allTypes.setByte(Byte.MAX_VALUE);
         byte b;
         if ((b = allTypes.getByte()) == Byte.MAX_VALUE)
         {
            getLog().debug(++test + "- Byte Max Value OK");
         }
         else
         {
            getLog().debug(++test + "- Byte Max Value Different (" + b + " <> " + Byte.MAX_VALUE + ")");
         }
      }
      catch (Exception e)
      {
         getLog().debug(++test + "- Byte Max Value Failed");
      }
      // Int
      try
      {
         allTypes.setInt(Integer.MIN_VALUE);
         int i;
         if ((i = allTypes.getInt()) == Integer.MIN_VALUE)
         {
            getLog().debug(++test + "- Int Min Value OK");
         }
         else
         {
            getLog().debug(++test + "- Int Min Value Different (" + i + " <> " + Integer.MIN_VALUE + ")");
         }
      }
      catch (Exception e)
      {
         getLog().debug(++test + "- Int Min Value Failed");
      }
      try
      {
         allTypes.setInt(Integer.MAX_VALUE);
         int i;
         if ((i = allTypes.getInt()) == Integer.MAX_VALUE)
         {
            getLog().debug(++test + "- Int Max Value OK");
         }
         else
         {
            getLog().debug(++test + "- Int Max Value Different (" + i + " <> " + Integer.MAX_VALUE + ")");
         }
      }
      catch (Exception e)
      {
         getLog().debug(++test + "- Int Max Value Failed");
      }
   }


   public static Test suite() throws Exception
   {
      return getDeploySetup(DbTypesUnitTestCase.class, "dbtest.jar");
   }

}
