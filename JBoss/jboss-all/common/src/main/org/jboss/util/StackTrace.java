/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util;

import java.io.IOException;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.PrintWriter;
import java.io.PrintStream;
import java.io.InputStreamReader;
import java.io.BufferedReader;
import java.io.Serializable;

import java.util.List;
import java.util.Iterator;
import java.util.ArrayList;

import org.jboss.util.stream.Printable;

/**
 * Provides access to the current stack trace by parsing the output of
 * <code>Throwable.printStackTrace()</code>.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public final class StackTrace
   implements Serializable, Cloneable, Printable
{
   /** Parse all entries */
   public static final int UNLIMITED = 0;

   /** Empty prefix constant */
   private static final String EMPTY_PREFIX = "";

   /** List of <tt>StackTrace.Entry</tt> elements */
   protected final List stack;

   /**
    * Initialize a <tt>StackTrace</tt>.
    *
    * @param detail  Detail throwable to determine stack entries from.
    * @param level   Number of levels to go down into the trace.
    * @param limit   The maximum number of entries to parse (does not
    *                include skipped levels or the description).
    *                A value <= zero results in all entries being parsed.
    *
    * @throws IllegalArgumentException    Invalid level or limit.
    * @throws NestedRuntimeException      Failed to create Parser.
    * @throws NestedRuntimeException      Failed to parse stack trace.
    */
   public StackTrace(final Throwable detail, 
                     final int level,
                     final int limit)
   {
      if (level < 0)
         throw new IllegalArgumentException("level < 0");
      if (limit < 0)
         throw new IllegalArgumentException("limit < 0");

      try {
         Parser parser = Parser.getInstance();
         stack = parser.parse(detail, level, limit);
      }
      catch (InstantiationException e) {
         throw new NestedRuntimeException(e);
      }
      catch (IOException e) {
         throw new NestedRuntimeException(e);
      }
   }

   /**
    * Initialize a <tt>StackTrace</tt>.
    *
    * @param detail  Detail throwable to determine stack entries from.
    * @param level   Number of levels to go down into the trace.
    *
    * @throws IllegalArgumentException    Invalid level.
    * @throws NestedRuntimeException      Failed to create Parser.
    * @throws NestedRuntimeException      Failed to parse stack trace.
    */
   public StackTrace(final Throwable detail, final int level) {
      this(detail, level, 0);
   }

   /**
    * Initialize a <tt>StackTrace</tt>.
    *
    * @param detail  Detail throwable to determine stack entries from.
    * @param level   Number of levels to go down into the trace.
    *
    * @throws NestedRuntimeException      Failed to create Parser.
    * @throws NestedRuntimeException      Failed to parse stack trace.
    */
   public StackTrace(final Throwable detail) {
      this(detail, 0, 0);
   }

   /**
    * Construct a <tt>StackTrace</tt>.
    *
    * @param level   Number of levels to go down into the trace.
    * @param limit   The maximum number of entries to parse (does not
    *                include skipped levels or the description).
    *                A value <= zero results in all entries being parsed.
    */
   public StackTrace(final int level, final int limit) {
      this(new Throwable(), level + 1, limit);
   }

   /**
    * Construct a <tt>StackTrace</tt>.
    *
    * @param level   Number of levels to go down into the trace.
    */
   public StackTrace(final int level) {
      this(new Throwable(), level + 1, UNLIMITED);
   }

   /**
    * Construct a <tt>StackTrace</tt>.
    */
   public StackTrace() {
      this(new Throwable(), 1, UNLIMITED);
   }

   /**
    * Sub-trace constructor.
    */
   protected StackTrace(final List stack) {
      this.stack = stack;
   }

   /**
    * Check if the given object is equals to this.
    *
    * @param obj  Object to test equality with.
    * @return     True if object is equal to this.
    */
   public boolean equals(final Object obj) {
      if (obj == this) return true;

      if (obj != null && obj.getClass() == getClass()) {
         return ((StackTrace)obj).stack.equals(stack);
      }
      
      return false;
   }
   
   /**
    * Returns a shallow cloned copy of this object.
    *
    * @return  A shallow cloned copy of this object.
    */
   public Object clone() {
      try {
         return super.clone();
      }
      catch (CloneNotSupportedException e) {
         throw new InternalError();
      }
   }

   /**
    * Returns the stack trace entry for the element at the given level.
    *
    * @param level   Number of levels.
    * @return        Stack trace entry.
    *
    * @throws IndexOutOfBoundsException   Invalid level index.
    */
   public Entry getEntry(final int level) {
      return (Entry)stack.get(level);
   }

   /**
    * Returns the stack trace entry for the calling method.
    *
    * @return  Stack trace entry for calling method.
    */
   public Entry getCallerEntry() {
      return getEntry(0);
   }

   /**
    * Return the root entry for this stack trace.
    *
    * @return  Stack trace entry for the root calling method.
    */
   public Entry getRootEntry() {
      return getEntry(stack.size() - 1);
   }

   /**
    * Returns a sub trace starting at the the given level.
    *
    * @param level   Number of levels.
    * @return        Sub-trace.
    */
   public StackTrace getSubTrace(final int level) {
      return new StackTrace(stack.subList(level, stack.size()));
   }

   /**
    * Returns a sub trace starting at the the given level.
    *
    * @param level   Number of levels.
    * @param limit   Limit the sub-trace.  If there are less entries
    *                than the limit, the limit has no effect.
    * @return        Sub-trace.
    */
   public StackTrace getSubTrace(final int level, int limit) {
      if (limit > 0) {
         limit = Math.min(level + limit, stack.size());
      }
      else {
         limit = stack.size();
      }
      // limit is now the ending index of the stack to return

      return new StackTrace(stack.subList(level, limit));
   }

   /**
    * Returns the stack trace starting at the calling method.
    *
    * @return  Stack trace for calling method.
    */
   public StackTrace getCallerTrace() {
      return getSubTrace(1);
   }

   /**
    * Print this stack trace.
    *
    * @param writer  The writer to print to.
    * @param prefix  Stack trace entry prefix.
    */
   public void print(final PrintWriter writer, final String prefix) {
      Iterator iter = stack.iterator();
      while (iter.hasNext()) {
         Entry entry = (Entry)iter.next();
         entry.print(writer, prefix);
      }
   }

   /**
    * Print this stack trace.
    *
    * @param writer  The writer to print to.
    */
   public void print(final PrintWriter writer) {
      print(writer, EMPTY_PREFIX);
   }

   /**
    * Print this stack trace.
    *
    * @param stream  The stream to print to.
    * @param prefix  Stack trace entry prefix.
    */
   public void print(final PrintStream stream, final String prefix) {
      Iterator iter = stack.iterator();
      while (iter.hasNext()) {
         Entry entry = (Entry)iter.next();
         entry.print(stream, prefix);
      }
   }

   /**
    * Print this stack trace.
    *
    * @param stream  The stream to print to.
    */
   public void print(final PrintStream stream) {
      print(stream, EMPTY_PREFIX);
   }

   /**
    * Print this stack trace to <code>System.err</code>.
    *
    * @param prefix  Stack trace entry prefix.
    */
   public void print(final String prefix) {
      print(System.err, prefix);
   }

   /**
    * Print this stack trace to <code>System.err</code>.
    */
   public void print() {
      print(System.err);
   }

   /**
    * Returns an iterator over all of the entries in the stack trace.
    *
    * @return  An iterator over all of the entries in the stack trace.
    */
   public Iterator iterator() {
      return stack.iterator();
   }

   /**
    * Returns the number of entries (or size) of the stack trace.
    *
    * @return  The number of entries (or size) of the stack trace.
    */
   public int size() {
      return stack.size();
   }


   /////////////////////////////////////////////////////////////////////////
   //                              Static Access                          //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Returns a stack trace entry for the current position in the stack.
    *
    * <p>The current entry refers to the method that has invoked
    *    {@link #currentEntry()}.
    *
    * @return  Current position in the stack.
    */
   public static final Entry currentEntry() {
      return new StackTrace().getCallerEntry();
   }

   /**
    * Returns a stack trace entry for the calling methods current position
    * in the stack.
    *
    * <p>Calling method in this case refers to the method that has called
    *    the method which invoked {@link #callerEntry()}.
    *
    * @return  Calling methods current position in the stack.
    *
    * @throws IndexOutOfBoundsException   The current entry is at bottom
    *                                     of the stack.
    */
   public static final Entry callerEntry() {
      return new StackTrace(1).getCallerEntry();
   }

   /**
    * Returns a stack trace entry for the root calling method of the current
    * execution thread.
    *
    * @return  Stack trace entry for the root calling method.
    */
   public static final Entry rootEntry() {
      return new StackTrace().getRootEntry();
   }


   /////////////////////////////////////////////////////////////////////////
   //                            StackTrace Entry                         //
   /////////////////////////////////////////////////////////////////////////

   /**
    * A stack trace entry.
    */
   public static final class Entry
      implements Cloneable, Serializable, Printable
   { 
      /** Unknown element token */
      public static final String UNKNOWN = "<unknown>";

      /** Default package token */
      public static final String DEFAULT = "<default>";

      /** The fully qualified class name for this entry */
      protected String className = UNKNOWN;

      /** The method name for this entry */
      protected String methodName = UNKNOWN;

      /** The source file name for this entry */
      protected String sourceFileName = UNKNOWN;

      /** The source file line number for this entry */
      protected String lineNumber = UNKNOWN;
      
      /**
       * Construct a new StackTrace entry.
       *
       * @param className        Fully qualified class name.
       * @param methodName       Method name.
       * @param sourceFileName   Source file name.
       * @param lineNumber       Source file line number.
       */
      public Entry(final String className,
                   final String methodName,
                   final String sourceFileName,
                   final String lineNumber)
      {
         this.className = className;
         this.methodName = methodName;
         this.sourceFileName = sourceFileName;
         this.lineNumber = lineNumber;
      }

      /**
       * Construct a new StackTrace entry.
       *
       * @param raw  The raw stack trace entry.
       */
      public Entry(final String raw) {
         // parse the raw string
         parse(raw);
      }

      /**
       * Parse a raw stack trace entry.
       *
       * @param raw  Raw stack trace.
       */
      protected void parse(final String raw) {
         // get the class and method names
         int j = raw.indexOf("at ") + 3;
         int i = raw.indexOf("(");
         if (j == -1 || i == -1) return;

         String temp = raw.substring(j, i);
         i = temp.lastIndexOf(".");
         if (i == -1) return;

         className  = temp.substring(0, i);
         methodName = temp.substring(i + 1);

         // get the source file name
         j = raw.indexOf("(") + 1;
         i = raw.indexOf(":");
         if (j == -1) return;
         if (i == -1) {
            i = raw.indexOf(")");
            if (i == -1) return;
            sourceFileName = raw.substring(j, i);
         }
         else {
            sourceFileName = raw.substring(j, i);
            // get the line number
            j = i + 1;
            i = raw.lastIndexOf(")");
            if (i != -1)
               lineNumber = raw.substring(j, i);
            else
               lineNumber = raw.substring(j);
         }
      }

      /**
       * Get the class name for this entry.
       *
       * @return  The class name for this entry.
       */
      public String getClassName() {
         return className;
      }

      /**
       * Get the short class name for this entry.
       *
       * <p>This is a macro for
       *    <code>Classes.stripPackageName(entry.getClassName())</code></p>
       *
       * @return  The short class name for this entry.
       */
      public String getShortClassName() {
         return Classes.stripPackageName(className);
      }

      /**
       * Get the method name for this entry.
       *
       * @return  The method name for this entry.
       */
      public String getMethodName() {
         return methodName;
      }

      /**
       * Get the fully qualified method name for this entry.
       *
       * <p>This is a macro for
       *    <code>entry.getClassName() + "." + entry.getMethodName()</code></p>
       *
       * @return  The fully qualified method name for this entry.
       */
      public String getFullMethodName() {
         return className + "." + methodName;
      }

      /**
       * Get the source file name for this entry.
       *
       * @return  The source file name for this entry.
       */
      public String getSourceFileName() {
         return sourceFileName;
      }

      /**
       * Get the source file line number for this entry.
       *
       * @return  The source file line number for this entry.
       */
      public String getLineNumber() {
         return lineNumber;
      }

      /**
       * Return a string representation of this with the given prefix.
       *
       * @param prefix  Prefix for returned string.
       * @return        A string in the format of 
       *                <code>prefixclassName.methodName(sourceFileName:lineNumber)</code>
       *                or <code>prefixclassName.methodName(sourceFileName)</code> if there
       *                is no line number.
       */
      public String toString(final String prefix) {
         StringBuffer buff = new StringBuffer();

         if (prefix != null)
            buff.append(prefix);

         buff.append(className).append(".").append(methodName)
            .append("(").append(sourceFileName);
         
         if (! lineNumber.equals(UNKNOWN))
            buff.append(":").append(lineNumber);

         buff.append(")");

         return buff.toString();
      }

      /**
       * Return a string representation of this.
       *
       * @return  A string in the format of 
       *          <code>className.methodName(sourceFileName:lineNumber)</code>
       */
      public String toString() {
         return toString(EMPTY_PREFIX);
      }

      /**
       * Return the hash code of this object.
       *
       * @return  The hash code of this object.
       */
      public int hashCode() {
         return HashCode.generate(new String[] {
            className, methodName, sourceFileName, lineNumber,
         });
      }

      /**
       * Check the equality of a given object with this.
       *
       * @param obj  Object to test equality with.
       * @return     True if the given object is equal to this.
       */
      public boolean equals(final Object obj) {
         if (obj == this) return true;

         if (obj != null && obj.getClass() == getClass()) {
            Entry entry = (Entry)obj;
            return
               entry.className.equals(className) &&
               entry.methodName.equals(methodName) &&
               entry.sourceFileName.equals(sourceFileName) &&
               entry.lineNumber.equals(lineNumber);
         }

         return false;
      }

      /**
       * Returns a shallow cloned copy of this object.
       *
       * @return  A shallow cloned copy of this object.
       */
      public Object clone() {
         try {
            return super.clone();
         }
         catch (CloneNotSupportedException e) {
            throw new InternalError();
         }
      }

      /**
       * Print this stack trace entry.
       *
       * @param writer  The writer to print to.
       * @param prefix  Prefix for string conversion.
       */
      public void print(final PrintWriter writer, final String prefix) {
         writer.println(this.toString(prefix));
      }

      /**
       * Print this stack trace entry.
       *
       * @param writer  The writer to print to.
       */
      public void print(final PrintWriter writer) {
         writer.println(this.toString());
      }

      /**
       * Print this stack trace entry.
       *
       * @param stream  The stream to print to.
       * @param prefix  Prefix for string conversion.
       */
      public void print(final PrintStream stream, final String prefix) {
         stream.println(this.toString(prefix));
      }

      /**
       * Print this stack trace entry.
       *
       * @param stream  The stream to print to.
       */
      public void print(final PrintStream stream) {
         stream.println(this.toString());
      }

      /**
       * Print this stack trace entry to <code>System.err<code>.
       *
       * @param prefix  Prefix for string conversion.
       */
      public void print(final String prefix) {
         print(System.err, prefix);
      }

      /**
       * Print this stack trace entry to <code>System.err<code>.
       */
      public void print() {
         print(System.err);
      }
   }


   /////////////////////////////////////////////////////////////////////////
   //                            StackTrace Parser                        //
   /////////////////////////////////////////////////////////////////////////

   /**
    * A parser which takes a standard Throwable and produces
    * {@link StackTrace.Entry} objects.
    */
   public static class Parser
   {
      /**
       * Skip the throwable description of the trace.
       *
       * @param reader  Reader representing the trace.
       *
       * @throws IOException
       */
      protected void skipDescription(final BufferedReader reader)
         throws IOException
      {
         reader.readLine();
      }

      /**
       * Skip to the correct level of the stack (going down into the stack).
       *
       * @param reader  Reader representing the stack trace.
       * @param level   Number of levels to go down.
       *
       * @throws IOException
       */
      protected void setLevel(final BufferedReader reader, final int level)
         throws IOException
      {
         for (int i=0; i<level; i++) {
            reader.readLine();
         }
      }

      /**
       * Read a throwable stack trace as an array of bytes.
       *
       * @param detail  Throwable to get trace from.
       * @return        Throwable stack trace as an array of bytes.
       *
       * @throws IOException
       */
      protected byte[] readBytes(final Throwable detail) throws IOException {
         ByteArrayOutputStream baos = new ByteArrayOutputStream();
         PrintStream ps = new PrintStream(baos);

         try {
            detail.printStackTrace(ps);
         }
         finally {
            ps.close();
         }

         return baos.toByteArray();
      }

      /**
       * Create a reader for the trace of the given Throwable.
       *
       * @param detail  Thorwable to get trace from.
       * @return        Reader for the throwable stack trace.
       *
       * @throws IOException
       */
      protected BufferedReader createReader(final Throwable detail)
         throws IOException
      {
         byte bytes[] = readBytes(detail);
         ByteArrayInputStream bais = new ByteArrayInputStream(bytes);
         InputStreamReader reader = new InputStreamReader(bais);

         return new BufferedReader(reader);
      }

      /**
       * Parse a Throwable stack trace.
       *
       * @param detail  Throwable to get trace from.
       * @param level   Number of levels down to begin parsing.
       * @param limit   The maximum number of entries to parse (does not
       *                include skipped levels or the description).
       *                A value <= zero results in all entries being parsed.
       * @return        List of {@link StackTrace.Entry} objects.
       *
       * @throws IOException
       */
      public List parse(final Throwable detail,
                        final int level,
                        final int limit) 
         throws IOException
      {
         // create an reader the throwable
         BufferedReader reader = createReader(detail);

         // ignore throwable description
         skipDescription(reader);

         // set the stack level
         setLevel(reader, level);

         // read in the stack entrys
         List list = new ArrayList();
         
         String raw;
         int count = 0;
         while ((raw = reader.readLine()) != null) {
            Entry entry = createEntry(raw);
            list.add(entry);

            // if we have reached the limit, then stop parsing
            if (limit > UNLIMITED && ++count >= limit) break;
         }
         
         return list;
      }

      /**
       * Create a stack trace entry for the given raw trace entry.
       *
       * @param raw  Raw stack trace entry.
       * @return     Stack trace entry.
       * 
       * @throws IOException
       */
      protected Entry createEntry(final String raw) throws IOException {
         return new Entry(raw);
      }
      
      //////////////////////////////////////////////////////////////////////
      //                          Singleton Access                        //
      //////////////////////////////////////////////////////////////////////

      /** The single instance of StackTrace.Parser */
      private static Parser instance = null;

      /**
       * Get the stack trace parser for this virtual machine.
       *
       * @return  Stack trace parser
       *
       * @throws InstantiationException
       */
      public static final synchronized Parser getInstance() 
         throws InstantiationException
      {
         if (instance == null) {
            // change to read parser class from a property
            instance = new Parser();
         }

         return instance;
      }
   }
}
