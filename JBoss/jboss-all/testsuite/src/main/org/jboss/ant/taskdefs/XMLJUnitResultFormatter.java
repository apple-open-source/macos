/*
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2000 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution, if
 *    any, must include the following acknowlegement:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowlegement may appear in the software itself,
 *    if and wherever such third-party acknowlegements normally appear.
 *
 * 4. The names "The Jakarta Project", "Ant", and "Apache Software
 *    Foundation" must not be used to endorse or promote products derived
 *    from this software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 */

package org.jboss.ant.taskdefs;

import java.io.*;
import java.text.CharacterIterator;
import java.text.StringCharacterIterator;
import java.util.*;
import javax.xml.parsers.*;
import org.w3c.dom.*;
import org.apache.tools.ant.BuildException;
import org.apache.tools.ant.util.DOMElementWriter;
import org.apache.tools.ant.taskdefs.optional.junit.JUnitResultFormatter;
import org.apache.tools.ant.taskdefs.optional.junit.JUnitTest;
import org.apache.tools.ant.taskdefs.optional.junit.XMLConstants;

import junit.framework.AssertionFailedError;
import junit.framework.Test;

/** A hack of the org.apache.tools.ant.taskdefs.optional.junit.XMLJUnitResultFormatter
 that dumps out the result file at the start of every test with the test marked
 as failed due to a timeout on entry to startTest. This is then removed
 and written out correctly in endTest.

 * @author <a href="mailto:stefan.bodewig@epost.de">Stefan Bodewig</a>
 * @author <a href="mailto:erik@hatcher.net">Erik Hatcher</a>
 * @author Scott.Stark@jboss.org
 *
 * @see FormatterElement
 */
public class XMLJUnitResultFormatter implements JUnitResultFormatter, XMLConstants
{
   
   private static DocumentBuilder getDocumentBuilder()
   {
      try
      {
         return DocumentBuilderFactory.newInstance().newDocumentBuilder();
      }
      catch(Exception exc)
      {
         throw new ExceptionInInitializerError(exc);
      }
   }
   
   /**
    * The XML document.
    */
   private Document doc;
   /**
    * The wrapper for the whole testsuite.
    */
   private Element rootElement;
   /**
    * Element for the current test.
    */
   private Element currentTest;
   /**
    * Timing helper.
    */
   private long lastTestStart = 0;
   /**
    * Where to write the log to.
    */
   private OutputStream out;
   protected JUnitTest currentSuite;
   protected File outputFile;
   protected long runs;
   protected long failures;
   protected long errors;
   protected long timeout;
   protected long startTime;
   protected boolean hadError;

   public XMLJUnitResultFormatter()
   {
   }
   
   public void setOutput(OutputStream out)
   {
      this.out = out;
   }
   
   public void setSystemOutput(String out)
   {
      formatOutput(SYSTEM_OUT, out);
   }
   
   public void setSystemError(String out)
   {
      formatOutput(SYSTEM_ERR, out);
   }
   
   /**
    * The whole testsuite started.
    */
   public void startTestSuite(JUnitTest suite)
   {
      doc = getDocumentBuilder().newDocument();
      rootElement = doc.createElement(TESTSUITE);
      rootElement.setAttribute(ATTR_NAME, suite.getName());
      
      // Output properties
      Element propsElement = doc.createElement(PROPERTIES);
      rootElement.appendChild(propsElement);
      Properties props = suite.getProperties();
      if (props != null)
      {
         Enumeration e = props.propertyNames();
         while (e.hasMoreElements())
         {
            String name = (String) e.nextElement();
            Element propElement = doc.createElement(PROPERTY);
            propElement.setAttribute(ATTR_NAME, name);
            propElement.setAttribute(ATTR_VALUE, props.getProperty(name));
            propsElement.appendChild(propElement);
         }
      }
      this.startTime = System.currentTimeMillis();
      this.currentSuite = suite;
      this.outputFile = this.getOutput();
      String junitTimeout = currentSuite.getProperties().getProperty("junit.timeout");
      this.timeout = Integer.parseInt(junitTimeout);
   }

   /**
    * The whole testsuite ended.
    */
   public void endTestSuite(JUnitTest suite) throws BuildException
   {
      rootElement.setAttribute(ATTR_TESTS, ""+suite.runCount());
      rootElement.setAttribute(ATTR_FAILURES, ""+suite.failureCount());
      rootElement.setAttribute(ATTR_ERRORS, ""+suite.errorCount());
      rootElement.setAttribute(ATTR_TIME, ""+(suite.getRunTime()/1000.0));
      try
      {
         out = new FileOutputStream(outputFile);
      }
      catch(IOException e)
      {
         throw new BuildException("Failed to open result file", e);
      }
      if (out != null)
      {
         Writer wri = null;
         try
         {
            wri = new OutputStreamWriter(out, "UTF8");
            wri.write("<?xml version=\"1.0\"?>\n");
            (new DOMElementWriter()).write(rootElement, wri, 0, "  ");
            wri.flush();
         }
         catch(IOException exc)
         {
            throw new BuildException("Unable to write log file", exc);
         }
         finally
         {
            if (out != System.out && out != System.err)
            {
               if (wri != null)
               {
                  try
                  {
                     wri.close();
                  }
                  catch (IOException e)
                  {
                  }
               }
            }
         }
      }
   }

   /**
    * Interface TestListener.
    *
    * <p>A new Test is started.
    */
   public void startTest(Test t)
   {    
      lastTestStart = System.currentTimeMillis();
      currentTest = doc.createElement(TESTCASE);
      currentTest.setAttribute(ATTR_NAME, t.toString());
      currentTest.setAttribute(ATTR_TIME, ""+(timeout / 1000.0));
      rootElement.appendChild(currentTest);

      runs ++;
      errors ++;
      hadError = false;
      // Mark this test as a timeout and write the result file
      internalFormatError(ERROR, t, new InternalError("Test timeout"));
      currentSuite.setCounts(runs, failures, errors);
      long elapsed = lastTestStart - startTime + timeout;
      currentSuite.setRunTime(elapsed);
      endTestSuite(currentSuite);
   }

   /**
    * Interface TestListener.
    *
    * <p>A Test is finished.
    */
   public void endTest(Test t)
   {
      errors --;
      if( hadError == false )
      {
         rootElement.removeChild(currentTest);
         currentTest = doc.createElement(TESTCASE);
         currentTest.setAttribute(ATTR_NAME, t.toString());
         rootElement.appendChild(currentTest);
      }

      long elapsed = System.currentTimeMillis()-lastTestStart;
      currentTest.setAttribute(ATTR_TIME, ""+(elapsed/ 1000.0));
   }

   /**
    * Interface TestListener for JUnit &lt;= 3.4.
    *
    * <p>A Test failed.
    */
   public void addFailure(Test test, Throwable t)
   {
      failures ++;
      formatError(FAILURE, test, t);
   }

   /**
    * Interface TestListener for JUnit &gt; 3.4.
    *
    * <p>A Test failed.
    */
   public void addFailure(Test test, AssertionFailedError t)
   {
      addFailure(test, (Throwable) t);
   }
   
   /**
    * Interface TestListener.
    *
    * <p>An error occured while running the test.
    */
   public void addError(Test test, Throwable t)
   {
      formatError(ERROR, test, t);
   }

   private void formatError(String type, Test test, Throwable t)
   {
      errors ++;
      hadError = true;
      internalFormatError(type, test, t);
   }

   private void internalFormatError(String type, Test test, Throwable t)
   {
      Element nested = doc.createElement(type);
      if (test != null)
      {
         if( currentTest == null )
         {
            currentTest = doc.createElement(TESTCASE);
            currentTest.setAttribute(ATTR_NAME, t.toString());
            rootElement.appendChild(currentTest);
         }
         else if( hadError == true )
         {
            Node err = currentTest.getLastChild();
            if( err != null )
               currentTest.removeChild(err);
         }
         currentTest.appendChild(nested);
      }
      else
      {
         rootElement.appendChild(nested);
      }

      String message = t.getMessage();
      if (message != null && message.length() > 0)
      {
         nested.setAttribute(ATTR_MESSAGE, t.getMessage());
      }
      nested.setAttribute(ATTR_TYPE, t.getClass().getName());
      
      StringWriter swr = new StringWriter();
      t.printStackTrace(new PrintWriter(swr, true));
      Text trace = doc.createTextNode(swr.toString());
      nested.appendChild(trace);
   }
   
   private void formatOutput(String type, String output)
   {
      Element nested = doc.createElement(type);
      rootElement.appendChild(nested);
      Text content = doc.createTextNode(output);
      nested.appendChild(content);
   }
   
   /** Since we are only given an OuptutStream, we have to figure out the
    file location based on the
    */
   protected File getOutput()
   {
      String filename = "TEST-" + currentSuite.getName() + ".xml";
      String reportsDir = currentSuite.getProperties().getProperty("build.reports");
      File destFile = new File(reportsDir, filename );
      String absFilename = destFile.getAbsolutePath();
      return destFile.getAbsoluteFile();
   }
} // XMLJUnitResultFormatter

