/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmx.compliance.standard;

import junit.framework.TestCase;

import javax.management.MBeanInfo;
import javax.management.MBeanOperationInfo;

/**
 * @author  <a href="mailto:trevor@protocool.com">Trevor Squires</a>.
 */

public class OperationInfoTEST extends TestCase
{
   private String failureHint;
   private MBeanInfo info;
   private String operationName;
   private int impact;
   private String returnType;
   private String signatureString;

   public OperationInfoTEST(String failureHint, MBeanInfo info, String operationName, int impact, String returnType, String[] signature)
   {
      super("testValidOperation");
      this.failureHint = failureHint;
      this.info = info;
      this.operationName = operationName;
      this.impact = impact;
      this.returnType = returnType;
      this.signatureString = InfoUtil.makeSignatureString(signature);
   }

   public void testValidOperation()
   {
      MBeanOperationInfo[] operations = info.getOperations();

      MBeanOperationInfo foundOperation = null;

      for (int i = 0; i < operations.length; i++)
      {
         if (operations[i].getName().equals(operationName))
         {
            if (signatureString.equals(InfoUtil.makeSignatureString(operations[i].getSignature())))
            {
               foundOperation = operations[i];
               break;
            }
         }
      }

      assertNotNull(failureHint + ": " + info.getClassName() + "." + operationName + signatureString + " was not found", foundOperation);
      assertEquals(failureHint + ": " + info.getClassName() + "." + operationName + signatureString + " impact", impact, foundOperation.getImpact());
      assertEquals(failureHint + ": " + info.getClassName() + "." + operationName + signatureString + " returnType", returnType, foundOperation.getReturnType());
   }
}
