package org.jboss.test.classloader.circularity.support;

/** <UserOfBase, L0>
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class UserOfBase
{
   /**
    *
    * @param s <Support,L1>
    */
   public void testBase(Support s)
   {
     // System.out.println("testBase.Base.class.CS: "
       //  + Base.class.getProtectionDomain().getCodeSource());
      // Base^L0 = Base^L1
      Base b = s.getBase();
      System.out.println("testBase.Base.CS: "
         + b.getClass().getProtectionDomain().getCodeSource());
      System.out.println("testBase.Support.CS: "
         + s.getClass().getProtectionDomain().getCodeSource());
   }
}
