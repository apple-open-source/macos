/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.proxy.ejb.handle;

import java.rmi.ServerException;
import java.lang.reflect.Method;
import java.io.ObjectStreamField;
import java.io.ObjectInputStream;
import java.io.IOException;
import java.io.ObjectOutputStream;
import java.util.Hashtable;

import javax.naming.InitialContext;
import javax.ejb.Handle;
import javax.ejb.EJBObject;
import javax.ejb.EJBHome;

import org.jboss.naming.NamingContextFactory;

/**
 * An EJB stateless session bean handle.
 *
 * @author  <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.2 $
 */
public class StatelessHandleImpl
      implements Handle
{
   /** Serial Version Identifier. */
   static final long serialVersionUID = 3811452873535097661L;
   private static final ObjectStreamField[] serialPersistentFields =
      new ObjectStreamField[]
   {
      new ObjectStreamField("jndiName", String.class),
      new ObjectStreamField("jndiEnv", Hashtable.class)
   };

   /** The JNDI name of the home inteface binding */
   private String jndiName;
   /** The JNDI env in effect when the home handle was created */
   private Hashtable jndiEnv;

   // Constructors --------------------------------------------------

   /**
    * Construct a <tt>StatelessHandleImpl</tt>.
    *
    * @param handle    The initial context handle that will be used
    *                  to restore the naming context or null to use
    *                  a fresh InitialContext object.
    * @param name      JNDI name.
    */
   public StatelessHandleImpl(String jndiName)
   {
      this.jndiName = jndiName;
      this.jndiEnv = (Hashtable) NamingContextFactory.lastInitialContextEnv.get();
   }

   // Public --------------------------------------------------------

   public EJBObject getEJBObject()
         throws ServerException
   {
      try
      {
         InitialContext ic = null;
         if( jndiEnv != null )
            ic = new InitialContext(jndiEnv);
         else
            ic = new InitialContext();
         EJBHome home = (EJBHome) ic.lookup(jndiName);
         Class type = home.getClass();
         Method method = type.getMethod("create", new Class[0]);
         return (EJBObject) method.invoke(home, new Object[0]);
      }
      catch (Exception e)
      {
         throw new ServerException("Could not get EJBObject", e);
      }
   }

   /**
    * @return the jndi name
    */
   public String getJNDIName()
   {
      return jndiName;
   }

   private void readObject(ObjectInputStream ois)
      throws IOException, ClassNotFoundException
   {
      ObjectInputStream.GetField getField = ois.readFields();
      jndiName = (String) getField.get("jndiName", null);
      jndiEnv = (Hashtable) getField.get("jndiEnv", null);
   }

   private void writeObject(ObjectOutputStream oos)
      throws IOException
   {
      ObjectOutputStream.PutField putField = oos.putFields();
      putField.put("jndiName", jndiName);
      putField.put("jndiEnv", jndiEnv);
      oos.writeFields();
   }
}
