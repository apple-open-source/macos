/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmq.test;

import java.io.ByteArrayInputStream;
import java.util.HashMap;

import org.jboss.mq.selectors.ISelectorParser;
import org.jboss.mq.selectors.Identifier;
import org.jboss.mq.selectors.Operator;
import org.jboss.mq.selectors.SelectorParser;
import org.jboss.test.JBossTestCase;

/** Tests of the JavaCC LL(1) parser.
 
 @author Scott.Stark@jboss.org
 @author d_jencks@users.sourceforge.net
 
 @version $Revision: 1.9.2.2 $
 
 * (david jencks)  Used constructor of SelectorParser taking a stream
 * to avoid reInit npe in all tests.  Changed to JBossTestCase and logging.
 */
public class SelectorParserUnitTestCase extends JBossTestCase
{
   static HashMap identifierMap = new HashMap();
   static ISelectorParser parser;
   
   public SelectorParserUnitTestCase(String name)
   {
      super(name);
   }
   
   protected void setUp() throws Exception
   {
      super.setUp();
      identifierMap.clear();
      if( parser == null )
      {
         parser = new SelectorParser(new ByteArrayInputStream(new byte[0]));
      }
   }
   
   public void testConstants() throws Exception
   {
      // String
      Object result = parser.parse("'A String'", identifierMap);
      getLog().debug("parse('A String') -> "+result);
      assertTrue("String is 'A String'", result.equals("A String"));
      // An identifier
      result = parser.parse("a_variable$", identifierMap);
      getLog().debug("parse(a_variable$) -> "+result);
      Identifier id = new Identifier("a_variable$");
      assertTrue("String is a_variable$", result.equals(id));
      // Long
      result = parser.parse("12345", identifierMap);
      getLog().debug("parse(12345) -> "+result);
      assertTrue("Long is 12345", result.equals(new Long(12345)));
      // Double
      result = parser.parse("12345.67", identifierMap);
      getLog().debug("parse(12345.67) -> "+result);
      assertTrue("Double is 12345.67", result.equals(new Double(12345.67)));
      // Boolean
      result = parser.parse("true", identifierMap);
      getLog().debug("parse(true) -> "+result);
      assertTrue("Boolean is true", result.equals(Boolean.TRUE));

      result = parser.parse("(true)", identifierMap);
      getLog().debug("parse((true)) -> "+result);
      assertTrue("Boolean is true", result.equals(Boolean.TRUE));

      result = parser.parse("FALSE", identifierMap);
      getLog().debug("parse(FALSE) -> "+result);
      assertTrue("Boolean is FALSE", result.equals(Boolean.FALSE));
   }
 
   public void testSimpleUnary() throws Exception
   {
      // Neg Long
      getLog().debug("parse(-12345 = -1 * 12345)");
      Operator result = (Operator) parser.parse("-12345 = -1 * 12345", identifierMap);
      getLog().debug("result -> "+result);
      Boolean b = (Boolean) result.apply();
      assertTrue("is true", b.booleanValue());

      // Neg Double
      getLog().debug("parse(-1 * 12345.67 = -12345.67)");
      result = (Operator) parser.parse("-1 * 12345.67 = -12345.67", identifierMap);
      getLog().debug("result -> "+result);
      b = (Boolean) result.apply();
      assertTrue("is true", b.booleanValue());

      getLog().debug("parse(-(1 * 12345.67) = -12345.67)");
      result = (Operator) parser.parse("-(1 * 12345.67) = -12345.67", identifierMap);
      getLog().debug("result -> "+result);
      b = (Boolean) result.apply();
      assertTrue("is true", b.booleanValue());
   }
   
   public void testPrecedenceNAssoc() throws Exception
   {
      getLog().debug("parse(4 + 2 * 3 / 2 = 7)");
      Operator result = (Operator) parser.parse("4 + 2 * 3 / 2 = 7", identifierMap);
      getLog().debug("result -> "+result);
      Boolean b = (Boolean) result.apply();
      assertTrue("is true", b.booleanValue());
      
      getLog().debug("parse(4 + ((2 * 3) / 2) = 7)");
      result = (Operator) parser.parse("4 + ((2 * 3) / 2) = 7", identifierMap);
      getLog().debug("result -> "+result);
      b = (Boolean) result.apply();
      assertTrue("is true", b.booleanValue());
      
      getLog().debug("parse(4 * -2 / -1 - 4 = 4)");
      result = (Operator) parser.parse("4 * -2 / -1 - 4 = 4", identifierMap);
      getLog().debug("result -> "+result);
      b = (Boolean) result.apply();
      assertTrue("is true", b.booleanValue());
      
      getLog().debug("parse(4 * ((-2 / -1) - 4) = -8)");
      result = (Operator) parser.parse("4 * ((-2 / -1) - 4) = -8", identifierMap);
      getLog().debug("result -> "+result);
      b = (Boolean) result.apply();
      assertTrue("is true", b.booleanValue());
   }
   
   public void testIds() throws Exception
   {
      getLog().debug("parse(a + b * c / d = e)");
      Operator result = (Operator) parser.parse("a + b * c / d = e", identifierMap);
      // 4 + 2 * 3 / 2 = 7
      Identifier a = (Identifier) identifierMap.get("a");
      a.setValue(new Long(4));
      Identifier b = (Identifier) identifierMap.get("b");
      b.setValue(new Long(2));
      Identifier c = (Identifier) identifierMap.get("c");
      c.setValue(new Long(3));
      Identifier d = (Identifier) identifierMap.get("d");
      d.setValue(new Long(2));
      Identifier e = (Identifier) identifierMap.get("e");
      e.setValue(new Long(7));
      getLog().debug("result -> "+result);
      Boolean bool = (Boolean) result.apply();
      assertTrue("is true", bool.booleanValue());
      
   }
   
   public void testTrueINOperator() throws Exception
   {
      getLog().debug("parse(Status IN ('new', 'cleared', 'acknowledged'))");
      Operator result = (Operator) parser.parse("Status IN ('new', 'cleared', 'acknowledged')", identifierMap);
      Identifier a = (Identifier) identifierMap.get("Status");
      a.setValue("new");
      getLog().debug("result -> "+result);
      Boolean bool = (Boolean) result.apply();
      assertTrue("is true", bool.booleanValue());
   }
   public void testFalseINOperator() throws Exception
   {
      getLog().debug("parse(Status IN ('new', 'cleared', 'acknowledged'))");
      Operator result = (Operator) parser.parse("Status IN ('new', 'cleared', 'acknowledged')", identifierMap);
      Identifier a = (Identifier) identifierMap.get("Status");
      a.setValue("none");
      getLog().debug("result -> "+result);
      Boolean bool = (Boolean) result.apply();
      assertTrue("is false", !bool.booleanValue());
   }
   
   public void testTrueOROperator() throws Exception
   {
      getLog().debug("parse((Status = 'new') OR (Status = 'cleared') OR (Status = 'acknowledged'))");
      Operator result = (Operator) parser.parse("(Status = 'new') OR (Status = 'cleared') OR (Status= 'acknowledged')", identifierMap);
      Identifier a = (Identifier) identifierMap.get("Status");
      a.setValue("new");
      getLog().debug("result -> "+result);
      Boolean bool = (Boolean) result.apply();
      assertTrue("is true", bool.booleanValue());
   }
   public void testFalseOROperator() throws Exception
   {
      getLog().debug("parse((Status = 'new') OR (Status = 'cleared') OR (Status = 'acknowledged'))");
      Operator result = (Operator) parser.parse("(Status = 'new') OR (Status = 'cleared') OR (Status = 'acknowledged')", identifierMap);
      Identifier a = (Identifier) identifierMap.get("Status");
      a.setValue("none");
      getLog().debug("result -> "+result);
      Boolean bool = (Boolean) result.apply();
      assertTrue("is false", !bool.booleanValue());
   }
   
   public void testInvalidSelector() throws Exception
   {
      getLog().debug("parse(definitely not a message selector!)");
      try
      {
         Object result = parser.parse("definitely not a message selector!", identifierMap);
         getLog().debug("result -> "+result);
         fail("Should throw an Exception.\n");
      }
      catch (Exception e)
      {
         getLog().info("testInvalidSelector failed as expected", e);
      }
   }
 
   /**
    * Test diffent syntax for approximate numeric literal (+6.2, -95.7, 7.)
    */
   public void testApproximateNumericLiteral1()
   {
      try
      {
         getLog().debug("parse(average = +6.2)");
         Object result = parser.parse("average = +6.2", identifierMap);
         getLog().debug("result -> "+result);
      } catch (Exception e)
      {
         fail(""+e);
      }
   }
   
   public void testApproximateNumericLiteral2()
   {
      try
      {
         getLog().debug("parse(average = -95.7)");
         Object result = parser.parse("average = -95.7", identifierMap);
         getLog().debug("result -> "+result);
      } catch (Exception e)
      {
         fail(""+e);
      }
   }
   public void testApproximateNumericLiteral3()
   {
      try
      {
         getLog().debug("parse(average = 7.)");
         Object result = parser.parse("average = 7.", identifierMap);
         getLog().debug("result -> "+result);
      } catch (Exception e)
      {
         fail(""+e);
      }
   }
   
   public void testGTExact()
   {
      try
      {
         getLog().debug("parse(weight > 2500)");
         Operator result = (Operator)parser.parse("weight > 2500", identifierMap);
         ((Identifier) identifierMap.get("weight")).setValue(new Integer(3000));
         getLog().debug("result -> "+result);
         Boolean bool = (Boolean) result.apply();
         assertTrue("is true", bool.booleanValue());
      } catch (Exception e)
      {
         getLog().debug("failed", e);
         fail(""+e);
      }
   }

   public void testGTFloat()
   {
      try
      {
         getLog().debug("parse(weight > 2500)");
         Operator result = (Operator)parser.parse("weight > 2500", identifierMap);
         ((Identifier) identifierMap.get("weight")).setValue(new Float(3000));
         getLog().debug("result -> "+result);
         Boolean bool = (Boolean) result.apply();
         assertTrue("is true", bool.booleanValue());
      } catch (Exception e)
      {
         getLog().debug("failed", e);
         fail(""+e);
      }
   }

   public void testLTDouble()
   {
      try
      {
         getLog().debug("parse(weight < 1.5)");
         Operator result = (Operator)parser.parse("weight < 1.5", identifierMap);
         ((Identifier) identifierMap.get("weight")).setValue(new Double(1.2));
         getLog().debug("result -> "+result);
         Boolean bool = (Boolean) result.apply();
         assertTrue("is true", bool.booleanValue());
      } catch (Exception e)
      {
         getLog().debug("failed", e);
         fail(""+e);
      }
   }

   public void testAndCombination()
   {
      try
      {
         getLog().debug("parse(JMSType = 'car' AND color = 'blue' AND weight > 2500)");
         Operator result = (Operator)parser.parse("JMSType = 'car' AND color = 'blue' AND weight > 2500", identifierMap);
         ((Identifier) identifierMap.get("JMSType")).setValue("car");
         ((Identifier) identifierMap.get("color")).setValue("blue");
         ((Identifier) identifierMap.get("weight")).setValue("3000");
         
         getLog().debug("result -> "+result);
         Boolean bool = (Boolean) result.apply();
         assertTrue("is false", !bool.booleanValue());
      } catch (Exception e)
      {
         getLog().debug("failed", e);
         fail(""+e);
      }
   }
   
   public void testINANDCombination()
   {
      try
      {
         getLog().debug("parse(Cateogry IN ('category1') AND Rating >= 2");
         Operator result = (Operator)parser.parse("Cateogry IN ('category1') AND Rating >= 2", identifierMap);
         ((Identifier) identifierMap.get("Cateogry")).setValue("category1");
         ((Identifier) identifierMap.get("Rating")).setValue(new Integer(3));
         getLog().debug("result -> "+result);
         Boolean bool = (Boolean) result.apply();
         assertTrue("is true", bool.booleanValue());
      } catch (Exception e)
      {
         getLog().debug("failed", e);
         fail(""+e);
      }
   }

   /** This testcase does not use the JBossServer so override
   the testServerFound to be a noop
   */
   public void testServerFound()
   {
   }

   public static void main(java.lang.String[] args)
   {
      junit.textui.TestRunner.run(SelectorParserUnitTestCase.class);
   }
}
