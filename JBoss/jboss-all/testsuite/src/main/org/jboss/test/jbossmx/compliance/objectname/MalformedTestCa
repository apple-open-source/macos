/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmx.compliance.objectname;

import org.jboss.test.jbossmx.compliance.TestCase;

import junit.framework.TestSuite;
import junit.framework.Test;

import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import java.util.Hashtable;

/**
 * Hammer ObjectName, making sure it spots all malformed inputs.
 *
 * This may look like overkill but it's not.  I want each
 * permutation to run independantly for full test coverage.
 *
 * This suite has twice as many tests (about 60) as my last
 * testcase - and for that it caught one extra bug for me.
 *
 * @author  <a href="mailto:trevor@protocool.com">Trevor Squires</a>.
 */
public class MalformedTestCase
   extends TestSuite
{
   public static final String GOOD_DOMAIN = "domain";
   public static final String GOOD_KEY = "key1";
   public static final String GOOD_VALUE = "val1";

   // strings containing illegal chars to use in key or value positions
   public static final String[] BAD_KEYVALS = {
      "",            // cannot be zero sized
      "som:thing",   // cannot contain domain separator
      "som?thing",   // cannot contain pattern chars
      "som*thing",   // cannot contain pattern chars
      "som,thing",   // cannot contain kvp chunk separator
      "som=thing",   // cannot contain kvp separator
   };

   // domains containing illegal domain chars
   public static final String[] BAD_DOMAINS = {
      "doma,in",    // , char in domain
      "doma=in",    // = char in domain
      "doma:in",    // : char in domain
   };

   // pre-cooked name strings dealing with structural malformations
   public static final String[] BAD_FULLNAMES = {
      "domain:key=val,key=val2",    // duplicate key
      "domain:=,foo=bar",           // both key and value empty
      "domain:key=val,,foo=bar",    // missing kvp in middle
      "domain:,key=val,foo=bar",    // missing kvp at beginning
      "domain:key=val,foo=bar,",    // missing kvp at end
      "domain:key=val,   ,foo=bar", // malformed kvp, no = char
   };

   public MalformedTestCase(String s)
   {
      super(s);
   }

   public static Test suite()
   {
      TestSuite suite = new TestSuite("All Malformed Tests");

      // Tests for nulls
      suite.addTest(new DomainKeyValueTEST(null, null, null));
      suite.addTest(new DomainKeyValueTEST(null, "key1", "val1"));
      suite.addTest(new DomainKeyValueTEST("domain", null, "val1"));
      suite.addTest(new DomainKeyValueTEST("domain", "key1", null));
      suite.addTest(new DomainKeyValueTEST("domain", null, null));
      suite.addTest(new DomainHashtableTEST(null, "key1", "val1"));

      // extra stuff related to null or zero sized hashtable
      suite.addTestSuite(DomainHashtableExtraTEST.class);

      // all illegal domain characters
      for (int i = 0; i < BAD_DOMAINS.length; i++)
      {
         suite.addTest(new FullNameTEST(BAD_DOMAINS[i] + ":" + GOOD_KEY + "=" + GOOD_VALUE));
         suite.addTest(new DomainKeyValueTEST(BAD_DOMAINS[i], GOOD_KEY, GOOD_VALUE));
         suite.addTest(new DomainHashtableTEST(BAD_DOMAINS[i], GOOD_KEY, GOOD_VALUE));
      }

      // all illegal key value characters
      for (int i = 0; i < BAD_KEYVALS.length; i++)
      {
         suite.addTest(new FullNameTEST(GOOD_DOMAIN + ":" + BAD_KEYVALS[i] + "=" + GOOD_VALUE));
         suite.addTest(new FullNameTEST(GOOD_DOMAIN + ":" + GOOD_KEY + "=" + BAD_KEYVALS[i]));
         suite.addTest(new DomainKeyValueTEST(GOOD_DOMAIN, BAD_KEYVALS[i], GOOD_VALUE));
         suite.addTest(new DomainKeyValueTEST(GOOD_DOMAIN, GOOD_KEY, BAD_KEYVALS[i]));
         suite.addTest(new DomainHashtableTEST(GOOD_DOMAIN, BAD_KEYVALS[i], GOOD_VALUE));
         suite.addTest(new DomainHashtableTEST(GOOD_DOMAIN, GOOD_KEY, BAD_KEYVALS[i]));
      }

      // all the structurally malformed fullnames
      for (int i = 0; i < BAD_FULLNAMES.length; i++)
      {
         suite.addTest( new FullNameTEST(BAD_FULLNAMES[i]));
      }

      return suite;
   }

   public static class FullNameTEST extends TestCase
   {
      private String fullName;

      public FullNameTEST(String fullName)
      {
         super("testMalformed");
         this.fullName = fullName;
      }

      public void testMalformed()
      {
         try
         {
            ObjectName name = new ObjectName(fullName);
         }
         catch (MalformedObjectNameException e)
         {
            return;
         }
         fail("expected a MalformedObjectNameException for: " + fullName);
      }
   }

   public static class DomainKeyValueTEST extends TestCase
   {
      private String domain;
      private String key;
      private String value;

      public DomainKeyValueTEST(String domain, String key, String value)
      {
         super("testMalformed");
         this.domain = domain;
         this.key = key;
         this.value = value;
      }

      public void testMalformed()
      {
         try
         {
            ObjectName name = new ObjectName(domain, key, value);
         }
         catch (MalformedObjectNameException e)
         {
            return;
         }
         fail("expected a MalformedObjectNameException for: " + domain + ":" + key + "=" + value);
      }
   }

   public static class DomainHashtableTEST extends TestCase
   {
      private String domain;
      private String key;
      private String value;

      public DomainHashtableTEST(String domain, String key, String value)
      {
         super("testMalformed");
         this.domain = domain;
         this.key = key;
         this.value = value;
      }

      public void testMalformed()
      {
         try
         {
            Hashtable h = new Hashtable();
            h.put(key, value);
            ObjectName name = new ObjectName(domain, h);
         }
         catch (MalformedObjectNameException e)
         {
            return;
         }
         fail("expected a MalformedObjectNameException for: " + domain + ":" + key + "=" + value);
      }
   }

   public static class DomainHashtableExtraTEST extends TestCase
   {
      public DomainHashtableExtraTEST(String s)
      {
         super(s);
      }

      public void testNullHashtable()
      {
         doCheck(GOOD_DOMAIN, null, "<null>");
      }

      public void testEmptyHashtable()
      {
         doCheck(GOOD_DOMAIN, new Hashtable(), "<empty_hashtable>");
      }

      public void testNonStringKey()
      {
         Hashtable h = new Hashtable();
         h.put(new Object(), GOOD_VALUE);
         doCheck(GOOD_DOMAIN, h, "<non_string_key>=" + GOOD_VALUE);
      }

      public void testNonStringValue()
      {
         Hashtable h = new Hashtable();
         h.put(GOOD_KEY, new Object());
         doCheck(GOOD_DOMAIN, h, GOOD_KEY + "=<non_string_value>");
      }

      private void doCheck(String domain, Hashtable h, String failureHint)
      {
         try
         {
            ObjectName name = new ObjectName(domain, h);
         }
         catch (MalformedObjectNameException e)
         {
            return;
         }
         fail("expected a MalformedObjectNameException for: " + domain + ":" + failureHint);
      }
   }
}
