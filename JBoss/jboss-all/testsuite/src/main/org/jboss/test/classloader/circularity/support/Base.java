package org.jboss.test.classloader.circularity.support;

/**
 *
 * @version $Revision: 1.1.4.1 $
 */
public class Base
{
   public static void run() throws Exception
   {
      System.out.println("Base.CS: "+Base.class.getProtectionDomain().getCodeSource());
      Class2 o2 = new Class2();
      System.out.println("o2.CS: "+o2.getClass().getProtectionDomain().getCodeSource());
      Class0 o0 = o2.get0();
      System.out.println("o0.CS: "+o0.getClass().getProtectionDomain().getCodeSource());
   }
}
