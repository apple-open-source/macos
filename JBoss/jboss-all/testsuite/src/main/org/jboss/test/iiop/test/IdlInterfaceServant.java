/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.iiop.test;

import org.jboss.test.iiop.interfaces.IdlInterface;
import org.jboss.test.iiop.interfaces.IdlInterfacePOA;

public class IdlInterfaceServant 
   extends IdlInterfacePOA
{
   public String echo (String s)
   {
      return s + " (echoed back)";
   }
   
}
