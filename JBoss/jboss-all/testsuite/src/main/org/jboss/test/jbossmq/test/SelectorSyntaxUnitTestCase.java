/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmq.test;

import junit.framework.TestCase;

import org.jboss.mq.SpyMessage;
import org.jboss.mq.selectors.Selector;

/**
 * Tests the complinace with the JMS Selector syntax.
 *
 * <p>Needs a lot of work...
 *
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @version $Revision: 1.3.4.1 $
 */
public class SelectorSyntaxUnitTestCase extends TestCase
{
   private Selector selector;
   private SpyMessage message;

   public SelectorSyntaxUnitTestCase(String name)
   {
      super(name);
   }

   protected void setUp() throws Exception
   {
      super.setUp();
      message = new SpyMessage();
   }

   public void testBooleanTrue() throws Exception
   {
      selector = new Selector("MyBoolean=true");
      testBoolean("MyBoolean", true);
   }

   public void testBooleanFalse() throws Exception
   {
      selector = new Selector("MyBoolean=false");
      testBoolean("MyBoolean", false);
   }

   private void testBoolean(String name, boolean flag) throws Exception
   {
      message.setBooleanProperty(name, flag);
      assertTrue(selector.test(message));

      message.setBooleanProperty(name, !flag);
      assertTrue(!selector.test(message));
   }

   public void testStringEquals() throws Exception
   {
      // First, simple test of string equality and inequality
      selector = new Selector("MyString='astring'");

      message.setStringProperty("MyString", "astring");
      assertTrue(selector.test(message));

      message.setStringProperty("MyString", "NOTastring");
      assertTrue(!selector.test(message));

      // test empty string
      selector = new Selector("MyString=''");

      message.setStringProperty("MyString", "");
      assertTrue("test 1", selector.test(message));

      message.setStringProperty("MyString", "NOTastring");
      assertTrue("test 2", !selector.test(message));

      // test literal apostrophes (which are escaped using two apostrophes
      // in selectors)
      selector = new Selector("MyString='test JBoss''s selector'");

      // note: apostrophes are not escaped in string properties
      message.setStringProperty("MyString", "test JBoss's selector");
      // this test fails -- bug 530120
      //assertTrue("test 3", selector.test(message));

      message.setStringProperty("MyString", "NOTastring");
      assertTrue("test 4", !selector.test(message));

   }

   public void testStringLike() throws Exception
   {
      // test LIKE operator with no wildcards
      selector = new Selector("MyString LIKE 'astring'");

      // test where LIKE operand matches
      message.setStringProperty("MyString", "astring");
      assertTrue(selector.test(message));

      // test one character string
      selector = new Selector("MyString LIKE 'a'");
      message.setStringProperty("MyString", "a");
      assertTrue(selector.test(message));

      // test empty string
      selector = new Selector("MyString LIKE ''");
      message.setStringProperty("MyString", "");
      assertTrue(selector.test(message));

      // tests where operand does not match
      selector = new Selector("MyString LIKE 'astring'");

      // test with extra characters at beginning
      message.setStringProperty("MyString", "NOTastring");
      assertTrue(!selector.test(message));

      // test with extra characters at end
      message.setStringProperty("MyString", "astringNOT");
      assertTrue(!selector.test(message));

      // test with extra characters in the middle
      message.setStringProperty("MyString", "astNOTring");
      assertTrue(!selector.test(message));

      // test where operand is entirely different
      message.setStringProperty("MyString", "totally different");
      assertTrue(!selector.test(message));

      // test case sensitivity
      message.setStringProperty("MyString", "ASTRING");
      assertTrue(!selector.test(message));

      // test empty string
      message.setStringProperty("MyString", "");
      assertTrue(!selector.test(message));

      // test lower-case 'like' operator?
   }

   public void testStringLikeUnderbarWildcard() throws Exception
   {
      // test LIKE operator with the _ wildcard, which
      // matches any single character

      // first, some tests with the wildcard by itself
      selector = new Selector("MyString LIKE '_'");

      // test match against single character
      message.setStringProperty("MyString", "a");
      assertTrue(selector.test(message));

      // test match failure against multiple characters
      message.setStringProperty("MyString", "aaaaa");
      assertTrue(!selector.test(message));

      // test match failure against the empty string
      message.setStringProperty("MyString", "");
      assertTrue(!selector.test(message));

      // next, tests with wildcard at the beginning of the string
      selector = new Selector("MyString LIKE '_bcdf'");

      // test match at beginning of string
      message.setStringProperty("MyString", "abcdf");
      assertTrue(selector.test(message));

      // match failure in first character after wildcard
      message.setStringProperty("MyString", "aXcdf");
      assertTrue(!selector.test(message));

      // match failure in middle character
      message.setStringProperty("MyString", "abXdf");
      assertTrue(!selector.test(message));

      // match failure in last character
      message.setStringProperty("MyString", "abcdX");
      assertTrue(!selector.test(message));

      // match failure with empty string
      message.setStringProperty("MyString", "");
      assertTrue(!selector.test(message));

      // match failure due to extra characters at beginning
      message.setStringProperty("MyString", "XXXabcdf");
      assertTrue(!selector.test(message));

      // match failure due to extra characters at the end
      message.setStringProperty("MyString", "abcdfXXX");
      assertTrue(!selector.test(message));

      // test that the _ wildcard does not match the 'empty' character
      message.setStringProperty("MyString", "bcdf");
      assertTrue(!selector.test(message));

      // next, tests with wildcard at the end of the string
      selector = new Selector("MyString LIKE 'abcd_'");

      // test match at end of string
      message.setStringProperty("MyString", "abcdf");
      assertTrue(selector.test(message));

      // match failure in first character before wildcard
      message.setStringProperty("MyString", "abcXf");
      assertTrue(!selector.test(message));

      // match failure in middle character
      message.setStringProperty("MyString", "abXdf");
      assertTrue(!selector.test(message));

      // match failure in first character
      message.setStringProperty("MyString", "Xbcdf");
      assertTrue(!selector.test(message));

      // match failure with empty string
      message.setStringProperty("MyString", "");
      assertTrue(!selector.test(message));

      // match failure due to extra characters at beginning
      message.setStringProperty("MyString", "XXXabcdf");
      assertTrue(!selector.test(message));

      // match failure due to extra characters at the end
      message.setStringProperty("MyString", "abcdfXXX");
      assertTrue(!selector.test(message));

      // test that the _ wildcard does not match the 'empty' character
      message.setStringProperty("MyString", "abcd");
      assertTrue(!selector.test(message));

      // test match in middle of string

      // next, tests with wildcard in the middle of the string
      selector = new Selector("MyString LIKE 'ab_df'");

      // test match in the middle of string
      message.setStringProperty("MyString", "abcdf");
      assertTrue(selector.test(message));

      // match failure in first character before wildcard
      message.setStringProperty("MyString", "aXcdf");
      assertTrue(!selector.test(message));

      // match failure in first character after wildcard
      message.setStringProperty("MyString", "abcXf");
      assertTrue(!selector.test(message));

      // match failure in last character
      message.setStringProperty("MyString", "abcdX");
      assertTrue(!selector.test(message));

      // match failure with empty string
      message.setStringProperty("MyString", "");
      assertTrue(!selector.test(message));

      // match failure due to extra characters at beginning
      message.setStringProperty("MyString", "XXXabcdf");
      assertTrue(!selector.test(message));

      // match failure due to extra characters at the end
      message.setStringProperty("MyString", "abcdfXXX");
      assertTrue(!selector.test(message));

      // test that the _ wildcard does not match the 'empty' character
      message.setStringProperty("MyString", "abdf");
      assertTrue(!selector.test(message));

      // test match failures
   }

   public void testStringLikePercentWildcard() throws Exception
   {
      // test LIKE operator with the % wildcard, which
      // matches any sequence of characters
      // note many of the tests are similar to those for _

      // first, some tests with the wildcard by itself
      selector = new Selector("MyString LIKE '%'");

      // test match against single character
      message.setStringProperty("MyString", "a");
      assertTrue(selector.test(message));

      // test match against multiple characters
      message.setStringProperty("MyString", "aaaaa");
      assertTrue(selector.test(message));

      message.setStringProperty("MyString", "abcdf");
      assertTrue(selector.test(message));

      // test match against the empty string
      message.setStringProperty("MyString", "");
      assertTrue(selector.test(message));

      // next, tests with wildcard at the beginning of the string
      selector = new Selector("MyString LIKE '%bcdf'");

      // test match with single character at beginning of string
      message.setStringProperty("MyString", "Xbcdf");
      assertTrue(selector.test(message));

      // match with multiple characters at beginning
      message.setStringProperty("MyString", "XXbcdf");
      assertTrue(selector.test(message));

      // match failure in middle character
      message.setStringProperty("MyString", "abXdf");
      assertTrue(!selector.test(message));

      // match failure in last character
      message.setStringProperty("MyString", "abcdX");
      assertTrue(!selector.test(message));

      // match failure with empty string
      message.setStringProperty("MyString", "");
      assertTrue(!selector.test(message));

      // match failure due to extra characters at the end
      message.setStringProperty("MyString", "abcdfXXX");
      assertTrue(!selector.test(message));

      // test that the % wildcard matches the empty string
      message.setStringProperty("MyString", "bcdf");
      assertTrue(selector.test(message));

      // next, tests with wildcard at the end of the string
      selector = new Selector("MyString LIKE 'abcd%'");

      // test match of single character at end of string
      message.setStringProperty("MyString", "abcdf");
      assertTrue(selector.test(message));

      // test match of multiple characters at end of string
      message.setStringProperty("MyString", "abcdfgh");
      assertTrue(selector.test(message));

      // match failure in first character before wildcard
      message.setStringProperty("MyString", "abcXf");
      assertTrue(!selector.test(message));

      // match failure in middle character
      message.setStringProperty("MyString", "abXdf");
      assertTrue(!selector.test(message));

      // match failure in first character
      message.setStringProperty("MyString", "Xbcdf");
      assertTrue(!selector.test(message));

      // match failure with empty string
      message.setStringProperty("MyString", "");
      assertTrue(!selector.test(message));

      // match failure due to extra characters at beginning
      message.setStringProperty("MyString", "XXXabcdf");
      assertTrue(!selector.test(message));

      // test that the % wildcard matches the empty string
      message.setStringProperty("MyString", "abcd");
      assertTrue(selector.test(message));

      // next, tests with wildcard in the middle of the string
      selector = new Selector("MyString LIKE 'ab%df'");

      // test match with single character in the middle of string
      message.setStringProperty("MyString", "abXdf");
      assertTrue(selector.test(message));

      // test match with multiple characters in the middle of string
      message.setStringProperty("MyString", "abXXXdf");
      assertTrue(selector.test(message));

      // match failure in first character before wildcard
      message.setStringProperty("MyString", "aXcdf");
      assertTrue(!selector.test(message));

      // match failure in first character after wildcard
      message.setStringProperty("MyString", "abcXf");
      assertTrue(!selector.test(message));

      // match failure in last character
      message.setStringProperty("MyString", "abcdX");
      assertTrue(!selector.test(message));

      // match failure with empty string
      message.setStringProperty("MyString", "");
      assertTrue(!selector.test(message));

      // match failure due to extra characters at beginning
      message.setStringProperty("MyString", "XXXabcdf");
      assertTrue(!selector.test(message));

      // match failure due to extra characters at the end
      message.setStringProperty("MyString", "abcdfXXX");
      assertTrue(!selector.test(message));

      // test that the % wildcard matches the empty string
      message.setStringProperty("MyString", "abdf");
      assertTrue(selector.test(message));

   }

   public void testStringLikePunctuation() throws Exception
   {
      // test proper handling of some punctuation characters.
      // non-trivial since the underlying implementation might
      // (and in fact currently does) use a general-purpose
      // RE library, which has a different notion of which
      // characters are wildcards

      // the particular tests here are motivated by the
      // wildcards of the current underlying RE engine,
      // GNU regexp.

      selector = new Selector("MyString LIKE 'a^$b'");
      message.setStringProperty("MyString", "a^$b");
      assertTrue(selector.test(message));

      // this one has a double backslash since backslash
      // is interpreted specially by Java
      selector = new Selector("MyString LIKE 'a\\dc'");
      message.setStringProperty("MyString", "a\\dc");
      assertTrue(selector.test(message));

      selector = new Selector("MyString LIKE 'a.c'");
      message.setStringProperty("MyString", "abc");
      assertTrue(!selector.test(message));

      selector = new Selector("MyString LIKE '[abc]'");
      message.setStringProperty("MyString", "[abc]");
      assertTrue(selector.test(message));

      selector = new Selector("MyString LIKE '[^abc]'");
      message.setStringProperty("MyString", "[^abc]");
      assertTrue(selector.test(message));

      selector = new Selector("MyString LIKE '[a-c]'");
      message.setStringProperty("MyString", "[a-c]");
      assertTrue(selector.test(message));

      selector = new Selector("MyString LIKE '[:alpha]'");
      message.setStringProperty("MyString", "[:alpha]");
      assertTrue(selector.test(message));

      selector = new Selector("MyString LIKE '(abc)'");
      message.setStringProperty("MyString", "(abc)");
      assertTrue(selector.test(message));

      selector = new Selector("MyString LIKE 'a|bc'");
      message.setStringProperty("MyString", "a|bc");
      assertTrue(selector.test(message));

      selector = new Selector("MyString LIKE '(abc)?'");
      message.setStringProperty("MyString", "(abc)?");
      assertTrue(selector.test(message));

      selector = new Selector("MyString LIKE '(abc)*'");
      message.setStringProperty("MyString", "(abc)*");
      assertTrue(selector.test(message));

      selector = new Selector("MyString LIKE '(abc)+'");
      message.setStringProperty("MyString", "(abc)+");
      assertTrue(selector.test(message));

      selector = new Selector("MyString LIKE '(abc){3}'");
      message.setStringProperty("MyString", "(abc){3}");
      assertTrue(selector.test(message));

      selector = new Selector("MyString LIKE '(abc){3,5}'");
      message.setStringProperty("MyString", "(abc){3,5}");
      assertTrue(selector.test(message));

      selector = new Selector("MyString LIKE '(abc){3,}'");
      message.setStringProperty("MyString", "(abc){3,}");
      assertTrue(selector.test(message));

      selector = new Selector("MyString LIKE '(?=abc)'");
      message.setStringProperty("MyString", "(?=abc)");
      assertTrue(selector.test(message));

      selector = new Selector("MyString LIKE '(?!abc)'");
      message.setStringProperty("MyString", "(?!abc)");
      assertTrue(selector.test(message));
   }
}
