/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.util.test;

import org.jboss.util.Strings;
import org.jboss.test.JBossTestCase;

/** 
 * Unit tests for the custom JBoss property editors
 *
 * @see org.jboss.util.Strings
 * @author <a href="Adrian.Brock@HappeningTimes.com">Adrian.Brock</a>
 * @version $Revision: 1.1.4.2 $
 */
public class PropertyPatternUnitTestCase extends JBossTestCase
{
   static final String simpleKey = "org.jboss.test.util.test.Simple";
   static final String simple = "AProperty";
   static final String anotherKey = "org.jboss.test.util.test.Another";
   static final String another = "BProperty";
   static final String doesNotExist = "org.jboss.test.util.test.DoesNotExist";
   static final String before = "Before";
   static final String between = "Between";
   static final String after = "After";

   String longWithNoProperties = new String("\n"+
"      BLOB_TYPE=OBJECT_BLOB\n"+
"      INSERT_TX = INSERT INTO JMS_TRANSACTIONS (TXID) values(?)\n"+
"      INSERT_MESSAGE = INSERT INTO JMS_MESSAGES (MESSAGEID, DESTINATION, MESSAGEBLOB, TXID, TXOP) VALUES(?,?,?,?,?)\n"+
"      SELECT_ALL_UNCOMMITED_TXS = SELECT TXID FROM JMS_TRANSACTIONS\n"+
"      SELECT_MAX_TX = SELECT MAX(TXID) FROM JMS_MESSAGES\n"+
"      SELECT_MESSAGES_IN_DEST = SELECT MESSAGEID, MESSAGEBLOB FROM JMS_MESSAGES WHERE DESTINATION=?\n"+
"      SELECT_MESSAGE = SELECT MESSAGEID, MESSAGEBLOB FROM JMS_MESSAGES WHERE MESSAGEID=? AND DESTINATION=?\n"+
"      MARK_MESSAGE = UPDATE JMS_MESSAGES SET TXID=?, TXOP=? WHERE MESSAGEID=? AND DESTINATION=?\n"+
"      UPDATE_MESSAGE = UPDATE JMS_MESSAGES SET MESSAGEBLOB=? WHERE MESSAGEID=? AND DESTINATION=?\n"+
"      UPDATE_MARKED_MESSAGES = UPDATE JMS_MESSAGES SET TXID=?, TXOP=? WHERE TXOP=?\n"+
"      UPDATE_MARKED_MESSAGES_WITH_TX = UPDATE JMS_MESSAGES SET TXID=?, TXOP=? WHERE TXOP=? AND TXID=?\n"+
"      DELETE_MARKED_MESSAGES_WITH_TX = DELETE FROM JMS_MESSAGES WHERE TXID IS NOT NULL AND TXOP=?\n"+
"      DELETE_TX = DELETE FROM JMS_TRANSACTIONS WHERE TXID = ?\n"+
"      DELETE_MARKED_MESSAGES = DELETE FROM JMS_MESSAGES WHERE TXID=? AND TXOP=?\n"+
"      DELETE_MESSAGE = DELETE FROM JMS_MESSAGES WHERE MESSAGEID=? AND DESTINATION=?\n"+
"      CREATE_MESSAGE_TABLE = CREATE TABLE JMS_MESSAGES ( MESSAGEID INTEGER NOT NULL, \\\n"+
"         DESTINATION VARCHAR(255) NOT NULL, TXID INTEGER, TXOP CHAR(1), \\\n"+
"         MESSAGEBLOB OBJECT, PRIMARY KEY (MESSAGEID, DESTINATION) )\n"+
"      CREATE_TX_TABLE = CREATE TABLE JMS_TRANSACTIONS ( TXID INTEGER )\n");

   static
   {
      System.setProperty(simpleKey, simple);
      System.setProperty(anotherKey, another);
   }

   public PropertyPatternUnitTestCase(String name)
   {
      super(name);
   }

   public void testEmptyPattern()
      throws Exception
   {
      assertEquals("Empty pattern", "",
         Strings.replaceProperties(""));
   }

   public void testNoPattern()
      throws Exception
   {
      assertEquals("No pattern", "xxx",
         Strings.replaceProperties("xxx"));
   }

   public void testSimpleProperty()
      throws Exception
   {
      assertEquals("Simple pattern", simple, 
         Strings.replaceProperties("${"+simpleKey+"}"));
   }

   public void testStringBeforeProperty()
      throws Exception
   {
      assertEquals("String before pattern", before + simple,
         Strings.replaceProperties(before + "${"+simpleKey+"}"));
   }

   public void testStringAfterProperty()
      throws Exception
   {
      assertEquals("String after pattern", simple + after,
         Strings.replaceProperties("${"+simpleKey+"}" + after));
   }

   public void testStringBeforeAfterProperty()
      throws Exception
   {
      assertEquals("String before and after pattern", before + simple + after,
         Strings.replaceProperties(before + "${"+simpleKey+"}" + after));
   }

   public void testStringBeforeBetweenProperty()
      throws Exception
   {
      assertEquals("String before and between pattern", before + simple + between + another,
         Strings.replaceProperties(before + "${"+simpleKey+"}" + between + "${" + anotherKey + "}"));
   }

   public void testStringAfterBetweenProperty()
      throws Exception
   {
      assertEquals("String after and between pattern", simple + between + another + after,
         Strings.replaceProperties("${"+simpleKey+"}" + between + "${" + anotherKey + "}" + after));
   }

   public void testStringBeforeAfterBetweenProperty()
      throws Exception
   {
      assertEquals("String before, after and between pattern", before + simple + between + another + after,
         Strings.replaceProperties(before + "${"+simpleKey+"}" + between + "${" + anotherKey + "}" + after)); 
   }

   public void testDollarBeforeProperty()
      throws Exception
   {
      assertEquals("Dollar before pattern", "$" + simple,
         Strings.replaceProperties("$${"+simpleKey+"}"));
   }

   public void testSpaceBetweenDollarAndProperty()
      throws Exception
   {
      assertEquals("Dollar before pattern", "$ {"+simpleKey+"}",
         Strings.replaceProperties("$ {"+simpleKey+"}"));
   }

   public void testPropertyDoesNotExist()
      throws Exception
   {
      assertEquals("Property does not exist", "${"+doesNotExist+"}",
         Strings.replaceProperties("${"+doesNotExist+"}"));
   }

   public void testPathologicalProperties()
      throws Exception
   {
      assertEquals("$", Strings.replaceProperties("$"));
      assertEquals("{", Strings.replaceProperties("{"));
      assertEquals("}", Strings.replaceProperties("}"));
      assertEquals("${", Strings.replaceProperties("${"));
      assertEquals("$}", Strings.replaceProperties("$}"));
      assertEquals("{$", Strings.replaceProperties("{$"));
      assertEquals("{}", Strings.replaceProperties("{}"));
      assertEquals("{{", Strings.replaceProperties("{{"));
      assertEquals("}$", Strings.replaceProperties("}$"));
      assertEquals("}{", Strings.replaceProperties("}{"));
      assertEquals("}}", Strings.replaceProperties("}}"));
      assertEquals("}}", Strings.replaceProperties("}}"));
      assertEquals("${}", Strings.replaceProperties("${}"));
      assertEquals("$}{", Strings.replaceProperties("$}{"));
      assertEquals("}${", Strings.replaceProperties("}${"));
      assertEquals("}{$", Strings.replaceProperties("}{$"));
      assertEquals("{$}", Strings.replaceProperties("{$}"));
      assertEquals("{}$", Strings.replaceProperties("{}$"));
   }

   public void testLongWithNoProperties()
      throws Exception
   {
      long start = System.currentTimeMillis();
      assertEquals("No properties in long string", longWithNoProperties,
         Strings.replaceProperties(longWithNoProperties));
      long end = System.currentTimeMillis();
      assertTrue("Shouldn't take very long", end - start < 1000);
   }

   /**
    * Override the testServerFound since these test don't need the JBoss server
    */
   public void testServerFound()
   {
   }

}

