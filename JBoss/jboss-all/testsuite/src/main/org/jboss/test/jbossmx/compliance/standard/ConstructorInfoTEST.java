/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmx.compliance.standard;

import junit.framework.TestCase;

import javax.management.MBeanConstructorInfo;
import javax.management.MBeanInfo;

/**
 * @author  <a href="mailto:trevor@protocool.com">Trevor Squires</a>.
 */

public class ConstructorInfoTEST extends TestCase
{
   private String failureHint;
   private MBeanInfo info;
   private String constructorName;
   private String signatureString;

   public ConstructorInfoTEST(String failureHint, MBeanInfo info, String constructorName, String[] signature)
   {
      super("testValidConstructor");
      this.failureHint = failureHint;
      this.info = info;
      this.constructorName = constructorName;
      this.signatureString = InfoUtil.makeSignatureString(signature);
   }

   public void testValidConstructor()
   {
      MBeanConstructorInfo[] constructors = info.getConstructors();

      MBeanConstructorInfo foundConstructor= null;

      for (int i = 0; i < constructors.length; i++)
      {
            if (signatureString.equals(InfoUtil.makeSignatureString(constructors[i].getSignature())))
            {
               foundConstructor = constructors[i];
               break;
            }
      }

      assertNotNull(failureHint + ": " + info.getClassName() + "." + constructorName + signatureString + " was not found", foundConstructor);
   }
}
