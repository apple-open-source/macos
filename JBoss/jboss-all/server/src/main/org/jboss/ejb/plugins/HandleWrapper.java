/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins;

import java.io.IOException;
import javax.ejb.Handle;

import org.jboss.invocation.MarshalledValue;

/** A wrapper for javax.ejb.Handle ivars of stateful sessions. This is needed
to prevent a handle being written out and then converted to the corresponding
EJBObject by the SessionObjectInputStream.resolveObject code since this
will result in a ClassCastException when the EJBObject is assigned to the
Handle ivar.

@author Scott.Stark@jboss.org
@version $Revision: 1.2 $
 */
public class HandleWrapper extends MarshalledValue
{
   public HandleWrapper()
   {
   }
   public HandleWrapper(Handle h) throws IOException
   {
      super(h);
   }

}

