
import java.util.HashMap;

import junit.framework.Test;
import junit.framework.TestCase;

import org.jboss.mq.selectors.Identifier;
import org.jboss.mq.selectors.Operator;
import org.jboss.mq.selectors.SelectorParser;
import org.jboss.mq.selectors.ISelectorParser;

/** Tests of the JavaCC LL(1) parser.
 @author Scott.Stark@jboss.org
 */
public class TestSelectorParser extends TestCase
{
    static HashMap identifierMap = new HashMap();
    static ISelectorParser parser;

    public TestSelectorParser(String name)
    {
       super(name);
    }

    protected void setUp() throws Exception
    {
       identifierMap.clear();
       if( parser == null )
       {
          parser = new SelectorParser();
       }
    }

    public void testConstants() throws Exception
    {
       // String
       Object result = parser.parse("'A String'", identifierMap);
       System.out.println("parse('A String') -> "+result);
       assert("String is 'A String'", result.equals("'A String'"));
       // An identifier
       result = parser.parse("a_variable$", identifierMap);
       System.out.println("parse(a_variable$) -> "+result);
       Identifier id = new Identifier("a_variable$");
       assert("String is a_variable$", result.equals(id));
       // Long
       result = parser.parse("12345", identifierMap);
       System.out.println("parse(12345) -> "+result);
       assert("Long is 12345", result.equals(new Long(12345)));
       // Double
       result = parser.parse("12345.67", identifierMap);
       System.out.println("parse(12345.67) -> "+result);
       assert("Double is 12345.67", result.equals(new Double(12345.67)));
    }

    public void testSimpleUnary() throws Exception
    {
       // Neg Long
       System.out.println("parse(-12345 = -1 * 12345)");
       Operator result = (Operator) parser.parse("-12345 = -1 * 12345", identifierMap);
       System.out.println("result -> "+result);
       Boolean b = (Boolean) result.apply();
       assert("is true", b.booleanValue());
       // Neg Double
       System.out.println("parse(-1 * 12345.67 = -12345.67)");
       result = (Operator) parser.parse("-1 * 12345.67 = -12345.67", identifierMap);
       System.out.println("result -> "+result);
       b = (Boolean) result.apply();
       assert("is true", b.booleanValue());
    }

    public void testPrecedenceNAssoc() throws Exception
    {
       System.out.println("parse(4 + 2 * 3 / 2 = 7)");
       Operator result = (Operator) parser.parse("4 + 2 * 3 / 2 = 7)", identifierMap);
       System.out.println("result -> "+result);
       Boolean b = (Boolean) result.apply();
       assert("is true", b.booleanValue());

       System.out.println("parse(4 + ((2 * 3) / 2) = 7)");
       result = (Operator) parser.parse("4 + ((2 * 3) / 2) = 7)", identifierMap);
       System.out.println("result -> "+result);
       b = (Boolean) result.apply();
       assert("is true", b.booleanValue());

       System.out.println("parse(4 * -2 / -1 - 4 = 4)");
       result = (Operator) parser.parse("4 * -2 / -1 - 4 = 4)", identifierMap);
       System.out.println("result -> "+result);
       b = (Boolean) result.apply();
       assert("is true", b.booleanValue());

       System.out.println("parse(4 * ((-2 / -1) - 4) = -8)");
       result = (Operator) parser.parse("4 * ((-2 / -1) - 4) = -8)", identifierMap);
       System.out.println("result -> "+result);
       b = (Boolean) result.apply();
       assert("is true", b.booleanValue());
    }

    public void testIds() throws Exception
    {
       System.out.println("parse(a + b * c / d = e)");
       Operator result = (Operator) parser.parse("a + b * c / d = e)", identifierMap);
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
       System.out.println("result -> "+result);
       Boolean bool = (Boolean) result.apply();
       assert("is true", bool.booleanValue());

    }

    public static void main(java.lang.String[] args)
    {
        junit.textui.TestRunner.run(TestSelectorParser.class);
    }
}
