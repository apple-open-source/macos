/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.util.test;

import java.beans.PropertyEditor;
import java.beans.PropertyEditorManager;
import java.io.File;
import java.net.URL;
import java.net.InetAddress;
import java.util.Comparator;
import java.util.Properties;
import javax.management.ObjectName;

import org.jboss.test.JBossTestCase;
import org.jboss.util.propertyeditor.ElementEditor;
import org.jboss.util.propertyeditor.DocumentEditor;

/** Unit tests for the custom JBoss property editors

@see org.jboss.util.propertyeditor.PropertyEditors
@author Scott.Stark@jboss.org
@version $Revision: 1.2.2.3 $
**/
public class PropertyEditorsUnitTestCase extends JBossTestCase
{
   /** Augment the PropertyEditorManager search path to incorporate the JBoss
    specific editors. This simply references the PropertyEditors.class to
    invoke its static initialization block.
    */
   static
   {
      Class c = org.jboss.util.propertyeditor.PropertyEditors.class;
   }

   static class StringArrayComparator implements Comparator
   {
      public int compare(Object o1, Object o2)
      {
         String[] a1 = (String[]) o1;
         String[] a2 = (String[]) o2;
         int compare = a1.length - a2.length;
         for(int n = 0; n < a1.length; n ++)
            compare += a1[n].compareTo(a2[n]);
         return compare;
      }
   }

   public PropertyEditorsUnitTestCase(String name)
   {
      super(name);
   }

   public void testEditorSearchPath()
      throws Exception
   {
      getLog().debug("+++ testEditorSearchPath");
      String[] searchPath = PropertyEditorManager.getEditorSearchPath();
      boolean foundJBossPath = false;
      for(int p = 0; p < searchPath.length; p ++)
      {
         String path = searchPath[p];
         getLog().debug("path["+p+"]="+path);
         foundJBossPath |= path.equals("org.jboss.util.propertyeditor");
      }
      assertTrue("Found org.jboss.util.propertyeditor in search path", foundJBossPath);
   }

   /** The mechanism for mapping java.lang.* variants of the primative types
    misses editors for java.lang.Boolean and java.lang.Integer. Here we test
    the java.lang.* variants we expect editors for.
    **/
   public void testJavaLangEditors()
      throws Exception
   {
      getLog().debug("+++ testJavaLangEditors");
      // The supported java.lang.* types
      Class[] types = {
         Boolean.class,
         Short.class,
         Integer.class,
         Long.class,
         Float.class,
         Double.class,
      };
      // The input string data for each type
      String[][] inputData = {
         {"true", "false", "TRUE", "FALSE", "tRuE", "FaLsE", null},
         {"1", "-1", "0"},
         {"1", "-1", "0"},
         {"1", "-1", "0", "1000"},
         {"1", "-1", "0", "1000.1"},
         {"1", "-1", "0", "1000.1"},
      };
      // The expected java.lang.* instance for each inputData value
      Object[][] expectedData = {
         {Boolean.TRUE, Boolean.FALSE, Boolean.TRUE, Boolean.FALSE, Boolean.TRUE, Boolean.FALSE, Boolean.FALSE},
         {Short.valueOf("1"), Short.valueOf("-1"), Short.valueOf("0")},
         {Integer.valueOf("1"), Integer.valueOf("-1"), Integer.valueOf("0")},
         {Long.valueOf("1"), Long.valueOf("-1"), Long.valueOf("0"), Long.valueOf("1000")},
         {Float.valueOf("1"), Float.valueOf("-1"), Float.valueOf("0"), Float.valueOf("1000.1")},
         {Double.valueOf("1"), Double.valueOf("-1"), Double.valueOf("0"), Double.valueOf("1000.1")},
      };
      Comparator[] comparators = new Comparator[types.length];

      doTests(types, inputData, expectedData, comparators);
   }

   /** Test custom JBoss property editors.
    **/
   public void testJBossEditors()
      throws Exception
   {
      getLog().debug("+++ testJBossEditors");
      Class[] types = {
         javax.management.ObjectName.class,
         java.util.Properties.class,
         java.io.File.class,
         java.net.URL.class,
         java.lang.Class.class,
         InetAddress.class,
         String[].class
      };
      // The input string data for each type
      String[][] inputData = {
         {"jboss.test:test=1"},
         {"prop1=value1\nprop2=value2"},
         {"/tmp/test1", "/tmp/subdir/../test2"},
         {"http://www.jboss.org"},
         {"java.util.Arrays"},
         // localhost must be defined for this to work
         {"127.0.0.1", "localhost"},
         {"1,2,3", "a,b,c"},
      };
      // The expected instance for each inputData value
      Properties props = new Properties();
      props.setProperty("prop1", "value1");
      props.setProperty("prop2", "value2");
      Object[][] expectedData = {
         {new ObjectName("jboss.test:test=1")},
         {props},
         {new File("/tmp/test1").getCanonicalFile(), new File("/tmp/test2").getCanonicalFile()},
         {new URL("http://www.jboss.org")},
         {java.util.Arrays.class},
         {InetAddress.getByName("127.0.0.1"), InetAddress.getByName("localhost")},
         {new String[]{"1", "2", "3"}, new String[] {"a", "b", "c"}},
      };
      // The Comparator for non-trival types
      Comparator[] comparators = {
         null, // ObjectName
         null, // Properties
         null, // File
         null, // URL
         null, // Class
         null, // InetAddress
         new StringArrayComparator(), // String[]
      };

      doTests(types, inputData, expectedData, comparators);
   }

   private void doTests(Class[] types, String[][] inputData, Object[][] expectedData,
      Comparator[] comparators)
   {
      for(int t = 0; t < types.length; t ++)
      {
         Class type = types[t];
         getLog().debug("Checking property editor for: "+type);
         PropertyEditor editor = PropertyEditorManager.findEditor(type);
         assertTrue("Found property editor for: "+type, editor != null);
         getLog().debug("Found property editor for: "+type+", editor="+editor);
         assertTrue("inputData.length == expectedData.length", inputData[t].length == expectedData[t].length);
         for(int i = 0; i < inputData[t].length; i ++)
         {
            String input = inputData[t][i];
            editor.setAsText(input);
            Object expected = expectedData[t][i];
            Object output = editor.getValue();
            Comparator c = comparators[t];
            boolean equals = false;
            if( c == null )
               equals = output.equals(expected);
            else
               equals = c.compare(output, expected) == 0;
            assertTrue("Transform of "+input+" equals "+expected+", output="+output, equals);
         }
      }
   }

   /** Override the testServerFound since these test don't need the JBoss server
    */
   public void testServerFound()
   {
   }

   /** Tests the DOM Document and Element editors.
    */
   public void testDocumentElementEditors()
   {
      getLog().debug("+++ testDocumentElementEditors");
      DocumentEditor de = new DocumentEditor();
      // Comments can appear outside of a document
      String s = "<!-- document --><doc name=\"whatever\"></doc>";
      de.setAsText(s);
      assertTrue("Document :\n" + de.getAsText(), de.getAsText().trim().endsWith(s));
      assertTrue(de.getValue() instanceof org.w3c.dom.Document);
      // Test whitespace preservation
      s = "<element>\n\n<e2></e2> testing\n\n</element>";
      de.setAsText(s);
      assertTrue("Document :\n" + de.getAsText() + "\nvs\n" + s, de.getAsText().trim().endsWith(s));

      ElementEditor ee = new ElementEditor();
      s = "<element>text</element>";
      ee.setAsText(s);
      assertEquals(s, ee.getAsText());
      assertTrue(ee.getValue() instanceof org.w3c.dom.Element);
   }

}

