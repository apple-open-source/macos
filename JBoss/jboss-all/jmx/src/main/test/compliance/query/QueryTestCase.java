/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.query;

import junit.framework.AssertionFailedError;
import junit.framework.TestCase;

import test.compliance.query.support.BooleanTest;
import test.compliance.query.support.NumberTest;
import test.compliance.query.support.StringTest;
import test.compliance.query.support.Trivial;

import java.util.Iterator;
import java.util.Set;
import java.util.HashSet;

import javax.management.AttributeValueExp;
import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.ObjectInstance;
import javax.management.ObjectName;
import javax.management.Query;
import javax.management.QueryExp;
import javax.management.StringValueExp;
import javax.management.ValueExp;

/**
 * Query unit tests
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class QueryTestCase
  extends TestCase
{
   // Attributes ----------------------------------------------------------------

   // Constructor ---------------------------------------------------------------

   /**
    * Construct the test
    */
   public QueryTestCase(String s)
   {
      super(s);
   }

   // Tests ---------------------------------------------------------------------

   /**
    * Test a boolean
    */
   public void testBoolean() throws Exception
   {
      ValueExp one = Query.value(true);
      ValueExp two = Query.value(false);
      equalsTEST(one, two);
      attrTEST(new BooleanTest(true), Query.attr("Boolean"), one, two);
      attrTEST(new BooleanTest(true), Query.attr(BooleanTest.class.getName(), "Boolean"), one, two);
      try
      {
         // Test in first
         new QueryTEST(
            new MBean[]
            {
               new MBean(new Trivial(), "Domain1:type=instance1")
            },
            new MBean[0],
            Query.in
            (
               one,
               new ValueExp[]
               {
                  one, two, two
               }
            )
         ).test();
         // Test in last
         new QueryTEST(
            new MBean[]
            {
               new MBean(new Trivial(), "Domain1:type=instance1")
            },
            new MBean[0],
            Query.in
            (
               one,
               new ValueExp[]
               {
                  two, two, one
               }
            )
         ).test();
         // Test in not the first or last
         new QueryTEST(
            new MBean[]
            {
               new MBean(new Trivial(), "Domain1:type=instance1")
            },
            new MBean[0],
            Query.in
            (
               one,
               new ValueExp[]
               {
                  two, one, two
               }
            )
         ).test();
         // Test not in
         new QueryTEST(
            new MBean[0],
            new MBean[]
            {
               new MBean(new Trivial(), "Domain1:type=instance1")
            },
            Query.in
            (
               one,
               new ValueExp[]
               {
                  two, two, two
               }
            )
         ).test();
      }
      catch (AssertionFailedError e)
      {
         fail("FAILS IN RI: Query.in boolean");
      }
   }

   /**
    * Test a double
    */
   public void testDouble() throws Exception
   {
      ValueExp one = Query.value(10d);
      ValueExp two = Query.value(20d);
      ValueExp div = Query.value(2d);
      ValueExp minus = Query.value(-10d);
      ValueExp mult = Query.value(200d);
      ValueExp plus = Query.value(30d);
      equalsTEST(one, two);
      operationTEST(one, two, div, minus, mult, plus);
      comparisonTEST(one, two);
      betweenTEST(one, two, plus);
      attrTEST(new NumberTest(10d), Query.attr("Number"), one, two);
      attrTEST(new NumberTest(10d), Query.attr(NumberTest.class.getName(), "Number"), one, two);
      inTEST(one, two, div, minus, mult, plus);
   }

   /**
    * Test a Double
    */
   public void testDoubleObject() throws Exception
   {
      ValueExp one = Query.value(new Double(10d));
      ValueExp two = Query.value(new Double(20d));
      ValueExp div = Query.value(new Double(2d));
      ValueExp minus = Query.value(new Double(-10d));
      ValueExp mult = Query.value(new Double(200d));
      ValueExp plus = Query.value(new Double(30d));
      equalsTEST(one, two);
      operationTEST(one, two, div, minus, mult, plus);
      comparisonTEST(one, two);
      betweenTEST(one, two, plus);
      attrTEST(new NumberTest(new Double(10d)), Query.attr("Number"), one, two);
      attrTEST(new NumberTest(new Double(10d)), Query.attr(NumberTest.class.getName(), "Number"), one, two);
      inTEST(one, two, div, minus, mult, plus);
   }

   /**
    * Test a float
    */
   public void testFloat() throws Exception
   {
      ValueExp one = Query.value(10f);
      ValueExp two = Query.value(20f);
      ValueExp div = Query.value(2f);
      ValueExp minus = Query.value(-10f);
      ValueExp mult = Query.value(200f);
      ValueExp plus = Query.value(30f);
      equalsTEST(one, two);
      operationTEST(one, two, div, minus, mult, plus);
      comparisonTEST(one, two);
      betweenTEST(one, two, plus);
      attrTEST(new NumberTest(10f), Query.attr("Number"), one, two);
      attrTEST(new NumberTest(10f), Query.attr(NumberTest.class.getName(), "Number"), one, two);
      inTEST(one, two, div, minus, mult, plus);
   }

   /**
    * Test a Float
    */
   public void testFloatObject() throws Exception
   {
      ValueExp one = Query.value(new Float(10f));
      ValueExp two = Query.value(new Float(20f));
      ValueExp div = Query.value(new Double(2f));
      ValueExp minus = Query.value(new Double(-10f));
      ValueExp mult = Query.value(new Double(200f));
      ValueExp plus = Query.value(new Double(30f));
      equalsTEST(one, two);
      operationTEST(one, two, div, minus, mult, plus);
      comparisonTEST(one, two);
      betweenTEST(one, two, plus);
      attrTEST(new NumberTest(new Float(10f)), Query.attr("Number"), one, two);
      attrTEST(new NumberTest(new Float(10f)), Query.attr(NumberTest.class.getName(), "Number"), one, two);
      inTEST(one, two, div, minus, mult, plus);
   }

   /**
    * Test a int
    */
   public void testInteger() throws Exception
   {
      ValueExp one = Query.value(10);
      ValueExp two = Query.value(20);
      ValueExp div = Query.value(2);
      ValueExp minus = Query.value(-10);
      ValueExp mult = Query.value(200);
      ValueExp plus = Query.value(30);
      equalsTEST(one, two);
      operationTEST(one, two, div, minus, mult, plus);
      comparisonTEST(one, two);
      betweenTEST(one, two, plus);
      attrTEST(new NumberTest(10), Query.attr("Number"), one, two);
      attrTEST(new NumberTest(10), Query.attr(NumberTest.class.getName(), "Number"), one, two);
      inTEST(one, two, div, minus, mult, plus);
   }

   /**
    * Test a Integer
    */
   public void testIntegerObject() throws Exception
   {
      ValueExp one = Query.value(new Integer(10));
      ValueExp two = Query.value(new Integer(20));
      ValueExp div = Query.value(new Double(2));
      ValueExp minus = Query.value(new Double(-10));
      ValueExp mult = Query.value(new Double(200));
      ValueExp plus = Query.value(new Double(30));
      equalsTEST(one, two);
      operationTEST(one, two, div, minus, mult, plus);
      comparisonTEST(one, two);
      betweenTEST(one, two, plus);
      attrTEST(new NumberTest(new Integer(10)), Query.attr("Number"), one, two);
      attrTEST(new NumberTest(new Integer(10)), Query.attr(NumberTest.class.getName(), "Number"), one, two);
      inTEST(one, two, div, minus, mult, plus);
   }

   /**
    * Test a long
    */
   public void testLong() throws Exception
   {
      ValueExp one = Query.value(10l);
      ValueExp two = Query.value(20l);
      ValueExp div = Query.value(2l);
      ValueExp minus = Query.value(-10l);
      ValueExp mult = Query.value(200l);
      ValueExp plus = Query.value(30l);
      equalsTEST(one, two);
      operationTEST(one, two, div, minus, mult, plus);
      comparisonTEST(one, two);
      betweenTEST(one, two, plus);
      attrTEST(new NumberTest(10l), Query.attr("Number"), one, two);
      attrTEST(new NumberTest(10l), Query.attr(NumberTest.class.getName(), "Number"), one, two);
      inTEST(one, two, div, minus, mult, plus);
   }

   /**
    * Test a Long
    */
   public void testLongObject() throws Exception
   {
      ValueExp one = Query.value(new Long(10l));
      ValueExp two = Query.value(new Long(20l));
      ValueExp div = Query.value(new Double(2l));
      ValueExp minus = Query.value(new Double(-10l));
      ValueExp mult = Query.value(new Double(200l));
      ValueExp plus = Query.value(new Double(30l));
      equalsTEST(one, two);
      operationTEST(one, two, div, minus, mult, plus);
      comparisonTEST(one, two);
      betweenTEST(one, two, plus);
      attrTEST(new NumberTest(new Long(10l)), Query.attr("Number"), one, two);
      attrTEST(new NumberTest(new Long(10l)), Query.attr(NumberTest.class.getName(), "Number"), one, two);
      inTEST(one, two, div, minus, mult, plus);
   }

   /**
    * Test a String
    */
   public void testString() throws Exception
   {
      ValueExp one = Query.value("Hello");
      ValueExp two = Query.value("Goodbye");
      ValueExp cat = Query.value("HelloGoodbye");
      ValueExp three = Query.value("ZZZZZZ");
      ValueExp four = Query.value("Hi");
      ValueExp five = Query.value("See ya");
      ValueExp six = Query.value("Laytaz");
      equalsTEST(one, two);
      catTEST(one, two, cat);
      comparisonTEST(two, one);
      betweenTEST(two, one, three);
      attrTEST(new StringTest("Hello"), Query.attr("String"), one, two);
      attrTEST(new StringTest("Hello"), Query.attr(StringTest.class.getName(), "String"), one, two);
      inTEST(one, two, three, four, five, six);
   }

   /**
    * Test and is true both
    */
   public void testAndTrueBoth() throws Exception
   {
      new QueryTEST(
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         new MBean[0],
         Query.and
         (
            Query.eq
            (
               Query.value(10), Query.value(10)
            ),
            Query.eq
            (
               Query.value("Hello"), Query.value("Hello")
            )
         )
      ).test();
   }

   /**
    * Test and is false first parameter
    */
   public void testAndFalseFirst() throws Exception
   {
      new QueryTEST(
         new MBean[0],
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         Query.and
         (
            Query.eq
            (
               Query.value(10), Query.value(20)
            ),
            Query.eq
            (
               Query.value("Hello"), Query.value("Hello")
            )
         )
      ).test();
   }

   /**
    * Test and is false second parameter
    */
   public void testAndFalseSecond() throws Exception
   {
      new QueryTEST(
         new MBean[0],
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         Query.and
         (
            Query.eq
            (
               Query.value(10), Query.value(10)
            ),
            Query.eq
            (
               Query.value("Hello"), Query.value("Goodbye")
            )
         )
      ).test();
   }

   /**
    * Test and is false both parameters
    */
   public void testAndFalseBoth() throws Exception
   {
      new QueryTEST(
         new MBean[0],
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         Query.and
         (
            Query.eq
            (
               Query.value(10), Query.value(20)
            ),
            Query.eq
            (
               Query.value("Hello"), Query.value("Goodbye")
            )
         )
      ).test();
   }

   /**
    * Test or is true both
    */
   public void testOrTrueBoth() throws Exception
   {
      new QueryTEST(
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         new MBean[0],
         Query.or
         (
            Query.eq
            (
               Query.value(10), Query.value(10)
            ),
            Query.eq
            (
               Query.value("Hello"), Query.value("Hello")
            )
         )
      ).test();
   }

   /**
    * Test or is false first parameter
    */
   public void testOrFalseFirst() throws Exception
   {
      new QueryTEST(
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         new MBean[0],
         Query.or
         (
            Query.eq
            (
               Query.value(10), Query.value(20)
            ),
            Query.eq
            (
               Query.value("Hello"), Query.value("Hello")
            )
         )
      ).test();
   }

   /**
    * Test or is false second parameter
    */
   public void testOrFalseSecond() throws Exception
   {
      new QueryTEST(
         new MBean[0],
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         Query.and
         (
            Query.eq
            (
               Query.value(10), Query.value(10)
            ),
            Query.eq
            (
               Query.value("Hello"), Query.value("Goodbye")
            )
         )
      ).test();
   }

   /**
    * Test or is false both parameters
    */
   public void testOrFalseBoth() throws Exception
   {
      new QueryTEST(
         new MBean[0],
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         Query.or
         (
            Query.eq
            (
               Query.value(10), Query.value(20)
            ),
            Query.eq
            (
               Query.value("Hello"), Query.value("Goodbye")
            )
         )
      ).test();
   }

   /**
    * Test not
    */
   public void testNot() throws Exception
   {
      new QueryTEST(
         new MBean[0],
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         Query.not
         (
            Query.eq
            (
               Query.value("Hello"), Query.value("Hello")
            )
         )
      ).test();
   }

   /**
    * Test not not
    */
   public void testNotNot() throws Exception
   {
      new QueryTEST(
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         new MBean[0],
         Query.not
         (
            Query.eq
            (
               Query.value("Hello"), Query.value("Goodbye")
            )
         )
      ).test();
   }

   /**
    * Test class attribute
    */
   public void testClassAttribute() throws Exception
   {
      new QueryTEST(
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         new MBean[]
         {
            new MBean(new Trivial(), "Domain1:type=instance2")
         },
         Query.eq
         (
            Query.classattr(), Query.value(NumberTest.class.getName())
         )
      ).test();
   }

   /**
    * Test any substring
    */
   public void testAnySubstring() throws Exception
   {
      new QueryTEST(
         new MBean[]
         {
            new MBean(new StringTest("Hello"), "Domain1:type=instance1"),
            new MBean(new StringTest("ellbeginning"), "Domain1:type=instance2"),
            new MBean(new StringTest("endell"), "Domain1:type=instance3"),
            new MBean(new StringTest("ell"), "Domain1:type=instance4")
         },
         new MBean[]
         {
            new MBean(new StringTest("Goodbye"), "Domain2:type=instance1"),
            new MBean(new StringTest("el"), "Domain2:type=instance2"),
            new MBean(new StringTest("ll"), "Domain2:type=instance3"),
            new MBean(new StringTest("e ll"), "Domain2:type=instance4"),
            new MBean(new StringTest("ELL"), "Domain2:type=instance5"),
            new MBean(new StringTest("Ell"), "Domain2:type=instance6")
         },
         Query.anySubString(Query.attr("String"), Query.value("ell"))
      ).test();
   }

   /**
    * Test final substring
    */
   public void testFinalSubstring() throws Exception
   {
      new QueryTEST(
         new MBean[]
         {
            new MBean(new StringTest("endell"), "Domain1:type=instance1"),
            new MBean(new StringTest("ell"), "Domain1:type=instance2")
         },
         new MBean[]
         {
            new MBean(new StringTest("Hello"), "Domain2:type=instance1"),
            new MBean(new StringTest("ellbeginning"), "Domain2:type=instance2"),
            new MBean(new StringTest("Goodbye"), "Domain2:type=instance3"),
            new MBean(new StringTest("el"), "Domain2:type=instance4"),
            new MBean(new StringTest("ll"), "Domain2:type=instance5"),
            new MBean(new StringTest("e ll"), "Domain2:type=instance6"),
            new MBean(new StringTest("ELL"), "Domain2:type=instance7"),
            new MBean(new StringTest("Ell"), "Domain2:type=instance8")
         },
         Query.finalSubString(Query.attr("String"), Query.value("ell"))
      ).test();
   }

   /**
    * Test initial substring
    */
   public void testInitialSubstring() throws Exception
   {
      new QueryTEST(
         new MBean[]
         {
            new MBean(new StringTest("ellbeginning"), "Domain1:type=instance1"),
            new MBean(new StringTest("ell"), "Domain1:type=instance2")
         },
         new MBean[]
         {
            new MBean(new StringTest("Hello"), "Domain2:type=instance1"),
            new MBean(new StringTest("endell"), "Domain2:type=instance2"),
            new MBean(new StringTest("Goodbye"), "Domain2:type=instance3"),
            new MBean(new StringTest("el"), "Domain2:type=instance4"),
            new MBean(new StringTest("ll"), "Domain2:type=instance5"),
            new MBean(new StringTest("e ll"), "Domain2:type=instance6"),
            new MBean(new StringTest("ELL"), "Domain2:type=instance7"),
            new MBean(new StringTest("Ell"), "Domain2:type=instance8")
         },
         Query.initialSubString(Query.attr("String"), Query.value("ell"))
      ).test();
   }

   /**
    * Test match asterisk beginning
    */
   public void testMatchAsteriskBeginning() throws Exception
   {
      new QueryTEST(
         new MBean[]
         {
            new MBean(new StringTest("endell"), "Domain1:type=instance1"),
            new MBean(new StringTest("ell"), "Domain1:type=instance2")
         },
         new MBean[]
         {
            new MBean(new StringTest("Hello"), "Domain2:type=instance1"),
            new MBean(new StringTest("ellbeginning"), "Domain2:type=instance2"),
            new MBean(new StringTest("Goodbye"), "Domain2:type=instance3"),
            new MBean(new StringTest("el"), "Domain2:type=instance4"),
            new MBean(new StringTest("ll"), "Domain2:type=instance5"),
            new MBean(new StringTest("e ll"), "Domain2:type=instance6"),
            new MBean(new StringTest("ELL"), "Domain2:type=instance7"),
            new MBean(new StringTest("Ell"), "Domain2:type=instance8")
         },
         Query.match(Query.attr("String"), Query.value("*ell"))
      ).test();
   }

   /**
    * Test match asterisk end
    */
   public void testMatchAsteriskEnd() throws Exception
   {
      new QueryTEST(
         new MBean[]
         {
            new MBean(new StringTest("ellbeginning"), "Domain1:type=instance1"),
            new MBean(new StringTest("ell"), "Domain1:type=instance2")
         },
         new MBean[]
         {
            new MBean(new StringTest("Hello"), "Domain2:type=instance1"),
            new MBean(new StringTest("beginningell"), "Domain2:type=instance2"),
            new MBean(new StringTest("Goodbye"), "Domain2:type=instance3"),
            new MBean(new StringTest("el"), "Domain2:type=instance4"),
            new MBean(new StringTest("ll"), "Domain2:type=instance5"),
            new MBean(new StringTest("e ll"), "Domain2:type=instance6"),
            new MBean(new StringTest("ELL"), "Domain2:type=instance7"),
            new MBean(new StringTest("Ell"), "Domain2:type=instance8")
         },
         Query.match(Query.attr("String"), Query.value("ell*"))
      ).test();
   }

   /**
    * Test any match asterisk beginning and end
    */
   public void testMatchAsteriskBeginningAndEnd() throws Exception
   {
      new QueryTEST(
         new MBean[]
         {
            new MBean(new StringTest("Hello"), "Domain1:type=instance1"),
            new MBean(new StringTest("ell"), "Domain1:type=instance2"),
            new MBean(new StringTest("endell"), "Domain1:type=instance3"),
            new MBean(new StringTest("beginningell"), "Domain1:type=instance4")
         },
         new MBean[]
         {
            new MBean(new StringTest("Goodbye"), "Domain2:type=instance1"),
            new MBean(new StringTest("el"), "Domain2:type=instance2"),
            new MBean(new StringTest("ll"), "Domain2:type=instance3"),
            new MBean(new StringTest("e ll"), "Domain2:type=instance4"),
            new MBean(new StringTest("ELL"), "Domain2:type=instance5"),
            new MBean(new StringTest("Ell"), "Domain2:type=instance6")
         },
         Query.match(Query.attr("String"), Query.value("*ell*"))
      ).test();
   }

   /**
    * Test match asterisk embedded
    */
   public void testMatchAsteriskEmbedded() throws Exception
   {
      new QueryTEST(
         new MBean[]
         {
            new MBean(new StringTest("Hello"), "Domain1:type=instance1"),
         },
         new MBean[]
         {
            new MBean(new StringTest("ell"), "Domain2:type=instance1"),
            new MBean(new StringTest("endell"), "Domain2:type=instance2"),
            new MBean(new StringTest("beginningell"), "Domain2:type=instance3"),
            new MBean(new StringTest("Goodbye"), "Domain2:type=instance4"),
            new MBean(new StringTest("el"), "Domain2:type=instance5"),
            new MBean(new StringTest("ll"), "Domain2:type=instance6"),
            new MBean(new StringTest("e ll"), "Domain4:type=instance7"),
            new MBean(new StringTest("ELL"), "Domain2:type=instance8"),
            new MBean(new StringTest("Ell"), "Domain2:type=instance9")
         },
         Query.match(Query.attr("String"), Query.value("H*o"))
      ).test();
   }

   /**
    * Test match question beginning
    */
   public void testMatchQuestionBeginning() throws Exception
   {
      new QueryTEST(
         new MBean[]
         {
            new MBean(new StringTest("Hello"), "Domain1:type=instance1"),
            new MBean(new StringTest("hello"), "Domain1:type=instance2"),
            new MBean(new StringTest(" ello"), "Domain1:type=instance3")
         },
         new MBean[]
         {
            new MBean(new StringTest("ello"), "Domain2:type=instance1"),
            new MBean(new StringTest("Ello"), "Domain2:type=instance2"),
            new MBean(new StringTest("hhello"), "Domain2:type=instance3"),
         },
         Query.match(Query.attr("String"), Query.value("?ello"))
      ).test();
   }

   /**
    * Test match question end
    */
   public void testMatchQuestionEnd() throws Exception
   {
      new QueryTEST(
         new MBean[]
         {
            new MBean(new StringTest("Hello"), "Domain1:type=instance1"),
            new MBean(new StringTest("HellO"), "Domain1:type=instance2"),
            new MBean(new StringTest("Hell "), "Domain1:type=instance3")
         },
         new MBean[]
         {
            new MBean(new StringTest("hell"), "Domain2:type=instance1"),
            new MBean(new StringTest("helL"), "Domain2:type=instance2"),
            new MBean(new StringTest("Helloo"), "Domain2:type=instance3"),
         },
         Query.match(Query.attr("String"), Query.value("Hell?"))
      ).test();
   }

   /**
    * Test match question beginning and end
    */
   public void testMatchQuestionBeginningEnd() throws Exception
   {
      new QueryTEST(
         new MBean[]
         {
            new MBean(new StringTest("Hello"), "Domain1:type=instance1"),
            new MBean(new StringTest("HellO"), "Domain1:type=instance2"),
            new MBean(new StringTest("hello"), "Domain1:type=instance3"),
            new MBean(new StringTest("hellO"), "Domain1:type=instance4"),
            new MBean(new StringTest(" ell "), "Domain1:type=instance5")
         },
         new MBean[]
         {
            new MBean(new StringTest("hell"), "Domain2:type=instance1"),
            new MBean(new StringTest("helL"), "Domain2:type=instance2"),
            new MBean(new StringTest("ello"), "Domain2:type=instance3"),
            new MBean(new StringTest("Ello"), "Domain2:type=instance4"),
            new MBean(new StringTest("ell"), "Domain2:type=instance5"),
            new MBean(new StringTest("Helloo"), "Domain2:type=instance6"),
            new MBean(new StringTest("HHello"), "Domain2:type=instance7"),
            new MBean(new StringTest("HHelloo"), "Domain2:type=instance8"),
         },
         Query.match(Query.attr("String"), Query.value("?ell?"))
      ).test();
   }

   /**
    * Test match question embedded
    */
   public void testMatchQuestionEmbedded() throws Exception
   {
      new QueryTEST(
         new MBean[]
         {
            new MBean(new StringTest("Hello"), "Domain1:type=instance1"),
            new MBean(new StringTest("HeLlo"), "Domain1:type=instance2"),
            new MBean(new StringTest("He lo"), "Domain1:type=instance3")
         },
         new MBean[]
         {
            new MBean(new StringTest("hell"), "Domain2:type=instance1"),
            new MBean(new StringTest("ello"), "Domain2:type=instance2"),
            new MBean(new StringTest("ell"), "Domain2:type=instance3"),
            new MBean(new StringTest("Helloo"), "Domain2:type=instance4"),
            new MBean(new StringTest("HHello"), "Domain2:type=instance5"),
            new MBean(new StringTest("HHelloo"), "Domain2:type=instance6"),
         },
         Query.match(Query.attr("String"), Query.value("He?lo"))
      ).test();
   }

   /**
    * Test match character set
    */
   public void testMatchCharacterSet() throws Exception
   {
      try
      {
         new QueryTEST(
            new MBean[]
            {
               new MBean(new StringTest("Hello"), "Domain1:type=instance1"),
               new MBean(new StringTest("HeLlo"), "Domain1:type=instance2"),
            },
            new MBean[]
            {
               new MBean(new StringTest("hell"), "Domain2:type=instance1"),
               new MBean(new StringTest("ello"), "Domain2:type=instance2"),
               new MBean(new StringTest("ell"), "Domain2:type=instance3"),
               new MBean(new StringTest("Helloo"), "Domain2:type=instance4"),
               new MBean(new StringTest("HHello"), "Domain2:type=instance5"),
               new MBean(new StringTest("HHelloo"), "Domain2:type=instance6"),
            },
            Query.match(Query.attr("String"), Query.value("He[lL]lo"))
         ).test();
      }
      catch (AssertionFailedError e)
      {
          fail("FAILS IN RI: expected Hello to match He[lL]lo");
      }
   }

   /**
    * Test match character range
    */
   public void testMatchCharacterRange() throws Exception
   {
      try
      {
        new QueryTEST(
            new MBean[]
            {
               new MBean(new StringTest("Hello"), "Domain1:type=instance1"),
               new MBean(new StringTest("Hemlo"), "Domain1:type=instance2"),
            },
            new MBean[]
            {
               new MBean(new StringTest("hell"), "Domain2:type=instance1"),
               new MBean(new StringTest("He lo"), "Domain2:type=instance2"),
               new MBean(new StringTest("Heklo"), "Domain2:type=instance3"),
               new MBean(new StringTest("Henlo"), "Domain2:type=instance4"),
               new MBean(new StringTest("HeLlo"), "Domain2:type=instance5"),
               new MBean(new StringTest("HeMlo"), "Domain2:type=instance6"),
            },
            Query.match(Query.attr("String"), Query.value("He[l-m]lo"))
         ).test();
      }
      catch (AssertionFailedError e)
      {
          fail("FAILS IN RI: didn't expected HeMlo to match He[l-m]lo");
      }
   }

   /**
    * Test match escaping question mark
    */
   public void testEscapingQuestion() throws Exception
   {
      new QueryTEST(
         new MBean[]
         {
            new MBean(new StringTest("Hello?"), "Domain1:type=instance1"),
         },
         new MBean[]
         {
            new MBean(new StringTest("Helloz"), "Domain2:type=instance1"),
         },
         Query.match(Query.attr("String"), Query.value("Hello\\?"))
      ).test();
   }

   /**
    * Test match escaping asterisk
    */
   public void testEscapingAsterisk() throws Exception
   {
      new QueryTEST(
         new MBean[]
         {
            new MBean(new StringTest("Hello*"), "Domain1:type=instance1"),
         },
         new MBean[]
         {
            new MBean(new StringTest("Helloz"), "Domain2:type=instance1"),
         },
         Query.match(Query.attr("String"), Query.value("Hello\\*"))
      ).test();
   }

   /**
    * Test match escaping open bracket
    */
   public void testEscapingOpenBracket() throws Exception
   {
      new QueryTEST(
         new MBean[]
         {
            new MBean(new StringTest("Hello[ab]"), "Domain1:type=instance1"),
         },
         new MBean[]
         {
            new MBean(new StringTest("Helloa"), "Domain2:type=instance1"),
            new MBean(new StringTest("Hello\\a"), "Domain2:type=instance2"),
         },
         Query.match(Query.attr("String"), Query.value("Hello\\[ab]"))
      ).test();
   }

   /**
    * Test match minus in character set
    */
   public void testMinusInCharacterSet() throws Exception
   {
      try
      {
         new QueryTEST(
            new MBean[]
            {
               new MBean(new StringTest("Hello-"), "Domain1:type=instance1"),
            },
            new MBean[]
            {
               new MBean(new StringTest("Hello[ab-]"), "Domain2:type=instance1"),
            },
            Query.match(Query.attr("String"), Query.value("Hello[ab-]"))
         ).test();
      }
      catch (AssertionFailedError e)
      {
          fail("FAILS IN RI: expected Hello- to match Hello[ab-]");
      }
   }

   /**
    * Test threading, tests that running the same query in multiple threads
    * works. This test might not catch a threading problem on every run.
    */
   public void testThreading() throws Exception
   {
      MBeanServer server1 = MBeanServerFactory.createMBeanServer("server1");
      MBeanServer server2 = MBeanServerFactory.createMBeanServer("server2");
      try
      {
         ObjectName name = new ObjectName("Domain1:type=instance1");
         NumberTest bean1 = new NumberTest(1);
         NumberTest bean2 = new NumberTest(2);
         server1.registerMBean(bean1, name);
         server2.registerMBean(bean2, name);
         QueryExp query = Query.eq(Query.attr("Number"), Query.value(2));
         QueryThread thread1 = new QueryThread(server1, query, 0);
         QueryThread thread2 = new QueryThread(server2, query, 1);
         thread1.start();
         thread2.start();
         thread1.join(10000);
         thread1.check();
         thread2.join(10000);
         thread2.check();
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server1);
         MBeanServerFactory.releaseMBeanServer(server2);
      }
   }

   /**
    * Test pathological
    */
   public void testPathological() throws Exception
   {
      try
      {
         new QueryTEST(
            new MBean[]
            {
               new MBean(new StringTest("Hello(?:.)"), "Domain1:type=instance1"),
            },
            new MBean[]
            {
               new MBean(new StringTest("Hellox"), "Domain2:type=instance1"),
            },
            Query.match(Query.attr("String"), Query.value("Hello(?:.)"))
         ).test();
      }
      catch (AssertionFailedError e)
      {
          fail("FAILS IN JBossMX: expected Hello(?:.) to match Hello(?:.)");
      }
   }

   // Support ----------------------------------------------------------------

   private void equalsTEST(ValueExp value1, ValueExp value2)
      throws Exception
   {
      // Test equals
      new QueryTEST(
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         new MBean[0],
         Query.eq
         (
            value1, value1
         )
      ).test();
      // Test not equals
      new QueryTEST(
         new MBean[0],
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         Query.eq
         (
            value1, value2
         )
      ).test();
   }

   private void operationTEST(ValueExp value1, ValueExp value2, ValueExp div,
                              ValueExp minus, ValueExp mult, ValueExp plus)
      throws Exception
   {
      // Test div
      new QueryTEST(
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         new MBean[0],
         Query.eq
         (
            Query.div(value2, value1),
            div
         )
      ).test();
      // Test minus
      new QueryTEST(
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         new MBean[0],
         Query.eq
         (
            Query.minus(value1, value2),
            minus
         )
      ).test();
      // Test mult
      new QueryTEST(
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         new MBean[0],
         Query.eq
         (
            Query.times(value1, value2),
            mult
         )
      ).test();
      // Test plus
      new QueryTEST(
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         new MBean[0],
         Query.eq
         (
            Query.plus(value1, value2),
            plus
         )
      ).test();
   }

   private void catTEST(ValueExp value1, ValueExp value2, ValueExp cat)
      throws Exception
   {
      // Test cat
      new QueryTEST(
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         new MBean[0],
         Query.eq
         (
            Query.plus(value1, value2),
            cat
         )
      ).test();
   }

   private void comparisonTEST(ValueExp value1, ValueExp value2)
      throws Exception
   {
      // Test greater than or equals (really greater than)
      new QueryTEST(
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         new MBean[0],
         Query.geq
         (
            value2, value1
         )
      ).test();
      // Test greater than or equals (really greater equals)
      new QueryTEST(
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         new MBean[0],
         Query.geq
         (
            value1, value1
         )
      ).test();
      // Test not greater than or equals
      new QueryTEST(
         new MBean[0],
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         Query.geq
         (
            value1, value2
         )
      ).test();
      // Test greater than
      new QueryTEST(
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         new MBean[0],
         Query.gt
         (
            value2, value1
         )
      ).test();
      // Test not greater than
      new QueryTEST(
         new MBean[0],
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         Query.gt
         (
            value1, value2
         )
      ).test();
      // Test greater than or equals (really greater than)
      new QueryTEST(
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         new MBean[0],
         Query.leq
         (
            value1, value2
         )
      ).test();
      // Test greater than or equals (really greater equals)
      new QueryTEST(
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         new MBean[0],
         Query.leq
         (
            value1, value1
         )
      ).test();
      // Test not greater than or equals
      new QueryTEST(
         new MBean[0],
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         Query.leq
         (
            value2, value1
         )
      ).test();
      // Test greater than
      new QueryTEST(
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         new MBean[0],
         Query.lt
         (
            value1, value2
         )
      ).test();
      // Test not greater than
      new QueryTEST(
         new MBean[0],
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         Query.lt
         (
            value2, value1
         )
      ).test();
   }

   private void betweenTEST(ValueExp value1, ValueExp value2, ValueExp value3)
      throws Exception
   {
      // Test between (really between)
      new QueryTEST(
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         new MBean[0],
         Query.between
         (
            value2, value1, value3
         )
      ).test();
      // Test between (equals first)
      new QueryTEST(
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         new MBean[0],
         Query.between
         (
            value2, value2, value3
         )
      ).test();
      // Test between (equals second)
      new QueryTEST(
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         new MBean[0],
         Query.between
         (
            value2, value1, value2
         )
      ).test();
      // Test between (equals both)
      new QueryTEST(
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         new MBean[0],
         Query.between
         (
            value2, value2, value2
         )
      ).test();
      // Test not between (first)
      new QueryTEST(
         new MBean[0],
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         Query.between
         (
            value1, value2, value3
         )
      ).test();
      // Test not between (second)
      new QueryTEST(
         new MBean[0],
         new MBean[]
         {
            new MBean(new NumberTest(0), "Domain1:type=instance1")
         },
         Query.between
         (
            value3, value1, value2
         )
      ).test();
   }

   private void attrTEST(Object mbean, AttributeValueExp attr, ValueExp value1, ValueExp value2)
      throws Exception
   {
      // Test true
      new QueryTEST(
         new MBean[]
         {
            new MBean(mbean, "Domain1:type=instance1")
         },
         new MBean[]
         {
            new MBean(new Trivial(), "Domain1:type=instance2")
         },
         Query.eq
         (
            attr, value1
         )
      ).test();
      // Test false
      new QueryTEST(
         new MBean[0],
         new MBean[]
         {
            new MBean(mbean, "Domain1:type=instance1")
         },
         Query.eq
         (
            attr, value2
         )
      ).test();
   }

   private void inTEST(ValueExp value1, ValueExp value2, ValueExp value3,
                       ValueExp value4, ValueExp value5, ValueExp value6)
      throws Exception
   {
      // Test in first
      new QueryTEST(
         new MBean[]
         {
            new MBean(new Trivial(), "Domain1:type=instance1")
         },
         new MBean[0],
         Query.in
         (
            value1,
            new ValueExp[]
            {
               value1, value2, value3, value4, value5, value6
            }
         )
      ).test();
      // Test in last
      new QueryTEST(
         new MBean[]
         {
            new MBean(new Trivial(), "Domain1:type=instance1")
         },
         new MBean[0],
         Query.in
         (
            value6,
            new ValueExp[]
            {
               value1, value2, value3, value4, value5, value6
            }
         )
      ).test();
      // Test in not the first or last
      new QueryTEST(
         new MBean[]
         {
            new MBean(new Trivial(), "Domain1:type=instance1")
         },
         new MBean[0],
         Query.in
         (
            value3,
            new ValueExp[]
            {
               value1, value2, value3, value4, value5, value6
            }
         )
      ).test();
      // Test not in
      new QueryTEST(
         new MBean[0],
         new MBean[]
         {
            new MBean(new Trivial(), "Domain1:type=instance1")
         },
         Query.in
         (
            value1,
            new ValueExp[]
            {
               value2, value3, value4, value5, value6
            }
         )
      ).test();
   }

   private class MBean
   {
      public Object mbean;
      public ObjectName objectName;
      public MBean(Object object, String name)
         throws Exception
      {
         this.mbean = object;
         this.objectName = new ObjectName(name);
      }
   }

   private class QueryTEST
   {
      HashSet expectedInstances = new HashSet();
      HashSet expectedNames = new HashSet();
      QueryExp queryExp;
      MBeanServer server;

      public QueryTEST(MBean[] expected, MBean[] others, QueryExp queryExp)
         throws Exception
      {
         this.queryExp = queryExp;

         server = MBeanServerFactory.createMBeanServer();

         for (int i = 0; i < expected.length; i++)
         {
            ObjectInstance instance = server.registerMBean(expected[i].mbean, 
                                                           expected[i].objectName);
            expectedInstances.add(instance);
            expectedNames.add(instance.getObjectName());
         }
         for (int i = 0; i < others.length; i++)
            server.registerMBean(others[i].mbean, others[i].objectName);
      }

      public void test()
         throws Exception
      {
         try
         {
            testQueryMBeans();
            testQueryNames();
         }
         finally
         {
            MBeanServerFactory.releaseMBeanServer(server);
         }
      }

      public void testQueryMBeans()
         throws Exception
      {
         Set result = server.queryMBeans(null, queryExp);

         Iterator iterator = result.iterator();
         while (iterator.hasNext())
         {
            ObjectInstance instance = (ObjectInstance) iterator.next();
            Iterator iterator2 = expectedInstances.iterator();
            boolean found = false;
            while (iterator2.hasNext())
            {
               if (iterator2.next().equals(instance))
               {
                  iterator2.remove();
                  found = true;
                  break;
               }
            }
            if (found == false && 
               instance.getObjectName().getDomain().equals("JMImplementation") == false)
               fail("Unexpected instance " + instance.getObjectName()
                   + "\nfor query " + queryExp);
         }

         iterator = expectedInstances.iterator();
         if (iterator.hasNext())
         {
            ObjectInstance instance = (ObjectInstance) iterator.next();
            fail("Expected instance " + instance.getObjectName()
                   + "\nfor query " + queryExp);
         }
      }

      public void testQueryNames()
         throws Exception
      {
         Set result = server.queryNames(null, queryExp);

         Iterator iterator = result.iterator();
         while (iterator.hasNext())
         {
            ObjectName name = (ObjectName) iterator.next();
            Iterator iterator2 = expectedNames.iterator();
            boolean found = false;
            while (iterator2.hasNext())
            {
               if (iterator2.next().equals(name))
               {
                  iterator2.remove();
                  found = true;
                  break;
               }
            }
            if (found == false &&
               name.getDomain().equals("JMImplementation") == false)
               fail("Unexpected name " + name
                    + "\nfor query " + queryExp);
         }

         iterator = expectedNames.iterator();
         if (iterator.hasNext())
         {
            fail("Expected instance " + iterator.next()
                   + "\nfor query " + queryExp);
         }
      }
   }

   public class QueryThread
      extends Thread
   {
      MBeanServer server;
      QueryExp query;
      int expected;
      int result;
      public QueryThread(MBeanServer server, QueryExp query, int expected)
      {
         this.server = server;
         this.query = query;
         this.expected = expected;
      }
      public int getExpected()
      {
         return expected;
      }
      public void check()
      {
         assertEquals(expected, result);
      }
      public void run()
      {
         for (int i = 0; i < 1000; i++)
         {
            result = server.queryNames(null, query).size();
            if (result != expected)
               return;
         }
      }
   }
}
