/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.iiop.interfaces;

/** 
 * An exception class for testing exception propagation over IIOP.
 */
public class NegativeArgumentException extends Exception
{
   int i;
   
   public NegativeArgumentException(int i) 
   {
      super("Negative argument: " + i);
      this.i = i;
   }
   
   public int getNegativeArgument() 
   {
      return i;
   }
   
}
