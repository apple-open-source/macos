/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.naming;

import java.util.Collections;
import java.util.HashMap;
import java.util.Hashtable;
import java.util.Map;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.Name;
import javax.naming.NameAlreadyBoundException;
import javax.naming.NameNotFoundException;
import javax.naming.NamingException;
import javax.naming.Reference;
import javax.naming.RefAddr;
import javax.naming.StringRefAddr;
import javax.naming.spi.ObjectFactory;

/** A utility class that allows one to bind a non-serializable object into a
local JNDI context. The binding will only be valid for the lifetime of the
VM in which the JNDI InitialContext lives. An example usage code snippet is:

<code>
    // The non-Serializable object to bind
    Object nonserializable = ...;
    // An arbitrary key to use in the StringRefAddr. The best key is the jndi
    // name that the object will be bound under.
    String key = ...;
    // This places nonserializable into the NonSerializableFactory hashmap under key
    NonSerializableFactory.rebind(key, nonserializable);

    Context ctx = new InitialContext();
    // Bind a reference to nonserializable using NonSerializableFactory as the ObjectFactory
    String className = nonserializable.getClass().getName();
    String factory = NonSerializableFactory.class.getName();
    StringRefAddr addr = new StringRefAddr("nns", key);
    Reference memoryRef = new Reference(className, addr, factory, null);
    ctx.rebind(key, memoryRef);
</code>

Or you can use the rebind(Context, String, Object) convience method to simplify
the number of steps to:
<code>
    Context ctx = new InitialContext();
    // The non-Serializable object to bind
    Object nonserializable = ...;
    // The jndiName that the object will be bound into ctx with
    String jndiName = ...;
    // This places nonserializable into the NonSerializableFactory hashmap under key
    NonSerializableFactory.rebind(ctx, jndiName, nonserializable);
</code>

To unbind the object, use the following code snippet:

<code>
	new InitialContext().unbind(key);
	NonSerializableFactory.unbind(key);
</code>

@see javax.naming.spi.ObjectFactory
@see #rebind(Context, String, Object)

@author <a href="mailto:Scott.Stark@jboss.org">Scott Stark</a>.
@version $Revision: 1.9 $
*/
public class NonSerializableFactory implements ObjectFactory
{
    private static Map wrapperMap = Collections.synchronizedMap(new HashMap());

    /** Place an object into the NonSerializableFactory namespace for subsequent
    access by getObject. There cannot be an already existing binding for key.

    @param key, the name to bind target under. This should typically be the
    name that will be used to bind target in the JNDI namespace, but it does
    not have to be.
    @param target, the non-Serializable object to bind.
    @throws NameAlreadyBoundException, thrown if key already exists in the
     NonSerializableFactory map
    */
    public static synchronized void bind(String key, Object target) throws NameAlreadyBoundException
    {
        if( wrapperMap.containsKey(key) == true )
            throw new NameAlreadyBoundException(key+" already exists in the NonSerializableFactory map");
        wrapperMap.put(key, target);
    }
    /** Place or replace an object in the NonSerializableFactory namespce
     for subsequent access by getObject. Any existing binding for key will be
     replaced by target.

    @param key, the name to bind target under. This should typically be the
    name that will be used to bind target in the JNDI namespace, but it does
    not have to be.
    @param target, the non-Serializable object to bind.
    */
    public static void rebind(String key, Object target)
    {
        wrapperMap.put(key, target);
    }

    /** Remove a binding from the NonSerializableFactory map.

    @param key, the key into the NonSerializableFactory map to remove.
    @throws NameNotFoundException, thrown if key does not exist in the
     NonSerializableFactory map
    */
    public static void unbind(String key) throws NameNotFoundException
    {
        if( wrapperMap.remove(key) == null )
            throw new NameNotFoundException(key+" was not found in the NonSerializableFactory map");
    }
    /** Remove a binding from the NonSerializableFactory map.

    @param name, the name for the key into NonSerializableFactory map to remove.
     The key is obtained as name.toString().
    @throws NameNotFoundException, thrown if key does not exist in the
     NonSerializableFactory map
    */
    public static void unbind(Name name) throws NameNotFoundException
    {
        String key = name.toString();
        if( wrapperMap.remove(key) == null )
            throw new NameNotFoundException(key+" was not found in the NonSerializableFactory map");
    }

    /** Lookup a value from the NonSerializableFactory map.
    @return the object bound to key is one exists, null otherwise.
    */
    public static Object lookup(String key)
    {
        Object value = wrapperMap.get(key);
        return value;
    }
    /** Lookup a value from the NonSerializableFactory map.
    @return the object bound to key is one exists, null otherwise.
    */
    public static Object lookup(Name name)
    {
        String key = name.toString();
        Object value = wrapperMap.get(key);
        return value;
    }

    /** A convience method that simplifies the process of rebinding a
        non-zerializable object into a JNDI context.

    @param ctx, the JNDI context to rebind to.
    @param key, the key to use in both the NonSerializableFactory map and JNDI. It
        must be a valid name for use in ctx.bind().
    @param target, the non-Serializable object to bind.
    @throws NamingException, thrown on failure to rebind key into ctx.
    */
    public static synchronized void rebind(Context ctx, String key, Object target) throws NamingException
    {
        NonSerializableFactory.rebind(key, target);
        // Bind a reference to target using NonSerializableFactory as the ObjectFactory
        String className = target.getClass().getName();
        String factory = NonSerializableFactory.class.getName();
        StringRefAddr addr = new StringRefAddr("nns", key);
        Reference memoryRef = new Reference(className, addr, factory, null);
        ctx.rebind(key, memoryRef);
    }

   /** A convience method that simplifies the process of rebinding a
    non-zerializable object into a JNDI context. This version binds the
    target object into the default IntitialContext using name path.

   @param name, the name to use as JNDI path name. The key into the
    NonSerializableFactory map is obtained from the toString() value of name.
    The name parameter cannot be a 0 length name.
    Any subcontexts between the root and the name must exist.
   @param target, the non-Serializable object to bind.
   @throws NamingException, thrown on failure to rebind key into ctx.
   */
   public static synchronized void rebind(Name name, Object target) throws NamingException
   {
      rebind(name, target, false);
   }

   /** A convience method that simplifies the process of rebinding a
    non-zerializable object into a JNDI context. This version binds the
    target object into the default IntitialContext using name path.

   @param name, the name to use as JNDI path name. The key into the
    NonSerializableFactory map is obtained from the toString() value of name.
    The name parameter cannot be a 0 length name.
   @param target, the non-Serializable object to bind.
   @param createSubcontexts, a flag indicating if subcontexts of name should
    be created if they do not already exist.
   @throws NamingException, thrown on failure to rebind key into ctx.
   */
   public static synchronized void rebind(Name name, Object target,
      boolean createSubcontexts) throws NamingException
   {
       String key = name.toString();
       InitialContext ctx = new InitialContext();
       if( createSubcontexts == true && name.size() > 1 )
       {
          int size = name.size() - 1;
          Util.createSubcontext(ctx, name.getPrefix(size));
       }
       rebind(ctx, key, target);
   }

// --- Begin ObjectFactory interface methods
    /** Transform the obj Reference bound into the JNDI namespace into the
    actual non-Serializable object.

    @param obj, the object bound in the JNDI namespace. This must be an implementation
    of javax.naming.Reference with a javax.naming.RefAddr of type "nns" whose
    content is the String key used to location the non-Serializable object in the 
    NonSerializableFactory map.
    @param name, ignored.
    @param nameCtx, ignored.
    @param env, ignored.

    @return the non-Serializable object associated with the obj Reference if one
    exists, null if one does not.
    */
    public Object getObjectInstance(Object obj, Name name, Context nameCtx, Hashtable env)
        throws Exception
    {	// Get the nns value from the Reference obj and use it as the map key
        Reference ref = (Reference) obj;
        RefAddr addr = ref.get("nns");
        String key = (String) addr.getContent();
        Object target = wrapperMap.get(key);
        return target;
    }
// --- End ObjectFactory interface methods
}
