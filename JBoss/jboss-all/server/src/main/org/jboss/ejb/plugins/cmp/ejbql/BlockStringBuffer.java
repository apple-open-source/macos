/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.ejbql;

import java.util.Iterator;
import java.util.LinkedList;

/**
 * A buffer simmilar to StringBuffer that works on string blocks instead 
 * of individual characters.  This eliminates excessive array allocation
 * and copying at the expense of removal and substring opperations. This
 * is a greate compromise as usually the only functions called on a
 * StringBuffer are append, length, and toString.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.2 $
 */                            
public final class BlockStringBuffer {
   private LinkedList list = new LinkedList();
   private int length;
   
   public BlockStringBuffer() {
   }

   public BlockStringBuffer append(boolean b) {
      String string = String.valueOf(b);
      length += string.length();
      list.addLast(string);
      return this;
   }

   public BlockStringBuffer append(char c) {
      String string = String.valueOf(c);
      length += string.length();
      list.addLast(string);
      return this;
   }

   public BlockStringBuffer append(char[] str) {
      String string = String.valueOf(str);
      length += string.length();
      list.addLast(string);
      return this;
   }

   public BlockStringBuffer append(char[] str, int offset, int len) {
      String string = String.valueOf(str, offset, len);
      length += string.length();
      list.addLast(string);
      return this;
   }

   public BlockStringBuffer append(double d) {
      String string = String.valueOf(d);
      length += string.length();
      list.addLast(string);
      return this;
   }

   public BlockStringBuffer append(float f) {
      String string = String.valueOf(f);
      length += string.length();
      list.addLast(string);
      return this;
   }

   public BlockStringBuffer append(int i) {
      String string = String.valueOf(i);
      length += string.length();
      list.addLast(string);
      return this;
   }

   public BlockStringBuffer append(long l) {
      String string = String.valueOf(l);
      length += string.length();
      list.addLast(string);
      return this;
   }

   public BlockStringBuffer append(Object obj) {
      if(obj instanceof String) {
         String string = (String)obj;
         length += string.length();
         list.addLast(string);
      } else if(obj instanceof BlockStringBuffer) {
         BlockStringBuffer buf = (BlockStringBuffer)obj;
         length += buf.length;
         list.addAll(buf.list);
      } else {
         String string = String.valueOf(obj);
         length += string.length();
         list.addLast(string);
      }
      return this;
   }

   public BlockStringBuffer prepend(boolean b) {
      String string = String.valueOf(b);
      length += string.length();
      list.addFirst(string);
      return this;
   }

   public BlockStringBuffer prepend(char c) {
      String string = String.valueOf(c);
      length += string.length();
      list.addFirst(string);
      return this;
   }

   public BlockStringBuffer prepend(char[] str) {
      String string = String.valueOf(str);
      length += string.length();
      list.addFirst(string);
      return this;
   }

   public BlockStringBuffer prepend(char[] str, int offset, int len) {
      String string = String.valueOf(str, offset, len);
      length += string.length();
      list.addFirst(string);
      return this;
   }

   public BlockStringBuffer prepend(double d) {
      String string = String.valueOf(d);
      length += string.length();
      list.addFirst(string);
      return this;
   }

   public BlockStringBuffer prepend(float f) {
      String string = String.valueOf(f);
      length += string.length();
      list.addFirst(string);
      return this;
   }

   public BlockStringBuffer prepend(int i) {
      String string = String.valueOf(i);
      length += string.length();
      list.addFirst(string);
      return this;
   }

   public BlockStringBuffer prepend(long l) {
      String string = String.valueOf(l);
      length += string.length();
      list.addFirst(string);
      return this;
   }

   public BlockStringBuffer prepend(Object obj) {
      if(obj instanceof String) {
         String string = (String)obj;
         length += string.length();
         list.addFirst(string);
      } else if(obj instanceof BlockStringBuffer) {
         BlockStringBuffer buf = (BlockStringBuffer)obj;
         length += buf.length;
         list.addAll(0, buf.list);
      } else {
         String string = String.valueOf(obj);
         length += string.length();
         list.addFirst(string);
      }
      return this;
   }

   public int length() {
      return length;
   }

   public int size() {
      return length;
   }

   public StringBuffer toStringBuffer() {
      // use a string buffer because it will share the final buffer
      // with the string object which avoids an allocate and copy
      StringBuffer buf = new StringBuffer(length);

      for(Iterator iter = list.iterator(); iter.hasNext(); ) {
         buf.append( (String)iter.next() );
      }
      return buf;
   }

   public String toString() {
      return toStringBuffer().toString();
   }
}
