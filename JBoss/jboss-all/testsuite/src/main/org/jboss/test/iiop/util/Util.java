/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.iiop.util;

import org.jboss.test.iiop.interfaces.Boo;
import org.jboss.test.iiop.interfaces.Foo;

public class Util
{
   public final static String STRING =
      "the quick brown fox jumps over the lazy dog";
   
   public static String primitiveTypesToString(boolean flag, char c, byte b,
                                               short s, int i, long l, 
                                               float f, double d)
   {
      String str = "flag:\t" + flag + "\n"
                 + "c:\t" + c + "\n"
                 + "b:\t" + b + "\n"
                 + "s:\t" + s + "\n"
                 + "i:\t" + i + "\n"
                 + "l:\t" + l + "\n"
                 + "f:\t" + f + "\n"
                 + "d:\t" + d + "\n";
      return str;
    }
   
   public static String echo(String s)
   {
      return s + " (echoed back)";
   }
   
   public static Foo echoFoo(Foo f)
   {
      Foo newFoo = new Foo(f.i, f.s);
      newFoo.i++;
      newFoo.s += " <";
      return newFoo;
   }
   
   public static Boo echoBoo(Boo f)
   {
      Boo newBoo = new Boo(f.id, f.name);
      newBoo.id += "+";
      newBoo.name += " <";
      return newBoo;
   }
   
}
