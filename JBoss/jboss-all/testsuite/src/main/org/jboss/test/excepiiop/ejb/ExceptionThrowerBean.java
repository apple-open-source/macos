/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.excepiiop.ejb;

import javax.ejb.EJBException;

import org.jboss.test.util.ejb.SessionSupport;
import org.jboss.test.excepiiop.interfaces.ExceptionThrower;
import org.jboss.test.excepiiop.interfaces.JavaException;
import org.jboss.test.excepiiop.interfaces.IdlException;

public class ExceptionThrowerBean
   extends SessionSupport
{
   public void throwException(int i)
      throws JavaException,IdlException  
   {
      if (i > 0)
         throw new JavaException(i, "" + i + " is positive");
      else if (i < 0)
         throw new IdlException(i, "" + i + " is negative");
      else
         return; // no exception
   }
}
