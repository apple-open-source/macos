/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.proxy.ejb.handle;

import java.lang.reflect.Method;
import java.rmi.RemoteException;
import java.rmi.ServerException;
import java.io.IOException;
import java.io.ObjectStreamField;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.util.Hashtable;

import javax.naming.InitialContext;

import javax.ejb.Handle;
import javax.ejb.EJBObject;
import javax.ejb.EJBHome;
import org.jboss.naming.NamingContextFactory;

/**
 * An EJB entity bean handle implementation.
 *
 * @author  <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>.
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.2 $
 */
public class EntityHandleImpl
      implements Handle
{
   // Constants -----------------------------------------------------

   /** Serial Version Identifier. */
   static final long serialVersionUID = -132866169652666721L;
   private static final ObjectStreamField[] serialPersistentFields =
      new ObjectStreamField[]
   {
      new ObjectStreamField("id", Object.class),
      new ObjectStreamField("jndiName", String.class),
      new ObjectStreamField("jndiEnv", Hashtable.class)
   };

   /** The primary key of the entity bean. */
   private Object id;
   /** The JNDI name of the home inteface binding */
   private String jndiName;
   /** The JNDI env in effect when the home handle was created */
   private Hashtable jndiEnv;

   // Constructors --------------------------------------------------

   /**
    * Construct a <tt>EntityHandleImpl</tt>.
    *
    * @param state     The initial context state that will be used
    *                  to restore the naming context or null to use
    *                  a fresh InitialContext object.
    * @param name      JNDI name.
    * @param id        Primary key of the entity.
    */
   public EntityHandleImpl(String jndiName, Object id)
   {
      this.jndiName = jndiName;
      this.id = id;
      this.jndiEnv = (Hashtable) NamingContextFactory.lastInitialContextEnv.get();
   }

   // Public --------------------------------------------------------

   /**
    * Handle implementation.
    *
    * @return  <tt>EJBObject</tt> reference.
    *
    * @throws ServerException    Could not get EJBObject.
    * @throws RemoteException
    */
   public EJBObject getEJBObject() throws RemoteException
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
         Method method = type.getMethod("findByPrimaryKey", new Class[]{id.getClass()});

         // call findByPrimary on the target
         return (EJBObject) method.invoke(home, new Object[]{id});
      }
      catch (Exception e)
      {
         throw new ServerException("Could not get EJBObject", e);
      }
   }

   /**
    * @return the primary key
    */
   public Object getID()
   {
      return id;
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
      id = getField.get("id", null);
      jndiName = (String) getField.get("jndiName", null);
      jndiEnv = (Hashtable) getField.get("jndiEnv", null);
   }

   private void writeObject(ObjectOutputStream oos)
      throws IOException
   {
      ObjectOutputStream.PutField putField = oos.putFields();
      putField.put("id", id);
      putField.put("jndiName", jndiName);
      putField.put("jndiEnv", jndiEnv);
      oos.writeFields();
   }

}
