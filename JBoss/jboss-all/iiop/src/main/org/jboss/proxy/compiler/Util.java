/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.proxy.compiler;

/**
 * Runtime utility class used by IIOP stub classes.
 *
 * @author Unknown
 * @author <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @version $Revision: 1.1 $
 */
public class Util
{
   public static final Object[] NOARGS = {};

   public static final ClassLoader NULLCL = null;

   public static Boolean wrap(boolean x) 
   {
      return new Boolean(x);
   }
   
   public static Byte wrap(byte x) 
   {
      return new Byte(x);
   }
   
   public static Character wrap(char x) 
   {
      return new Character(x);
   }
   
   public static Short wrap(short x) 
   {
      return new Short(x);
   }
   
   public static Integer wrap(int x) 
   {
      return new Integer(x);
   }
   
   public static Long wrap(long x) 
   {
      return new Long(x);
   }
   
   public static Float wrap(float x) 
   {
      return new Float(x);
   }
   
   public static Double wrap(double x) 
   {
      return new Double(x);
   }
   
}
