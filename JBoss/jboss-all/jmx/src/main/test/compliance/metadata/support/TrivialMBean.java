/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.metadata.support;

public interface TrivialMBean
{
   void setSomething(String thing);

   String getSomething();

   void setSomethingInvalid(String thing);
   
   String getSomethingInvalid(Object invalid);
 
   void setSomethingInvalid2(String thing);
   
   void getSomethingInvalid2();
   
   void doOperation(String arg);
}
