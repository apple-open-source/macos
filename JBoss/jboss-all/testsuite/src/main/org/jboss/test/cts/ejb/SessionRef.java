package org.jboss.test.cts.ejb;

import java.io.Serializable;
import javax.ejb.Handle;

/** A serializable object that holds a reference to a session handle
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.2 $
 */
public class SessionRef implements Serializable
{
   Handle sessionHandle;

   /** Creates a new instance of SessionRef */
   public SessionRef(Handle sessionHandle)
   {
      this.sessionHandle = sessionHandle;
   }
   public Handle getHandle()
   {
      return sessionHandle;
   }
}

