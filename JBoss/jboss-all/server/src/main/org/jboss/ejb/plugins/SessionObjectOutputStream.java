/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins;

import java.io.OutputStream;
import java.io.ObjectOutputStream;
import java.io.IOException;
import java.rmi.Remote;
import java.rmi.server.RemoteObject;
import java.rmi.server.RemoteStub;
import javax.ejb.EJBObject;
import javax.ejb.EJBHome;
import javax.ejb.Handle;
import javax.ejb.SessionContext;
import javax.transaction.UserTransaction;


/**
 * The SessionObjectOutputStream is used to serialize stateful session beans when they are passivated
 *      
 * @see org.jboss.ejb.plugins.SessionObjectInputStream
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard berg</a>
 * @author <a href="mailto:sebastien.alborini@m4x.org">Sebastien Alborini</a>
 * @version $Revision: 1.8 $
 */
public class SessionObjectOutputStream
   extends ObjectOutputStream
{
   // Constructors -------------------------------------------------
   public SessionObjectOutputStream(OutputStream out)
      throws IOException
   {
      super(out);
      enableReplaceObject(true);
   }
      
   // ObjectOutputStream overrides ----------------------------------
   protected Object replaceObject(Object obj)
      throws IOException
   {
      Object replacement = obj;
      // section 6.4.1 of the ejb1.1 specification states what must be taken care of 
      
      // ejb reference (remote interface) : store handle
      if (obj instanceof EJBObject)
         replacement = ((EJBObject)obj).getHandle();
      
      // ejb reference (home interface) : store handle
      else if (obj instanceof EJBHome)
         replacement = ((EJBHome)obj).getHomeHandle();
      
      // session context : store a typed dummy object
      else if (obj instanceof SessionContext)
         replacement = new StatefulSessionBeanField(StatefulSessionBeanField.SESSION_CONTEXT);

      // naming context : the jnp implementation is serializable, do nothing

      // user transaction : store a typed dummy object
      else if (obj instanceof UserTransaction)
         replacement = new StatefulSessionBeanField(StatefulSessionBeanField.USER_TRANSACTION);      

      else if( obj instanceof Handle )
         replacement = new HandleWrapper((Handle)obj);

      else if( (obj instanceof Remote) && !(obj instanceof RemoteStub) )
      {
         Remote remote = (Remote) obj;
         try
         {
            replacement = RemoteObject.toStub(remote);
         }
         catch(IOException ignore)
         {
            // Let the Serialization layer try with original object
         }
      }

      return replacement;
   }
}

