package org.jboss.test.classloader.circularity.support;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class Support
{
   private Base base = new Base();

   public Base getBase()
   {
      return base;
   }
}
