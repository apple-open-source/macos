/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.naming;

import javax.naming.Context;
import javax.naming.Name;
import javax.naming.NameNotFoundException;
import javax.naming.NamingException;

/** A static utility class for common JNDI operations.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.8 $
 */
public class Util
{

    /** Create a subcontext including any intermediate contexts.
    @param ctx, the parent JNDI Context under which value will be bound
    @param name, the name relative to ctx of the subcontext.
    @return The new or existing JNDI subcontext
    @throws NamingException, on any JNDI failure
    */
    public static Context createSubcontext(Context ctx, String name)
        throws NamingException
    {
        Name n = ctx.getNameParser("").parse(name);
        return createSubcontext(ctx, n);
    }
    /** Create a subcontext including any intermediate contexts.
    @param ctx, the parent JNDI Context under which value will be bound
    @param name, the name relative to ctx of the subcontext.
    @return The new or existing JNDI subcontext
    @throws NamingException, on any JNDI failure
    */
    public static Context createSubcontext(Context ctx, Name name)
        throws NamingException
    {
        Context subctx = ctx;
        for(int pos = 0; pos < name.size(); pos ++)
        {
            String ctxName = name.get(pos);
            try
            {
                subctx = (Context) ctx.lookup(ctxName);
            }
            catch(NameNotFoundException e)
            {
                subctx = ctx.createSubcontext(ctxName);
            }
            // The current subctx will be the ctx for the next name component
            ctx = subctx;
        }
        return subctx;
    }

    /** Bind val to name in ctx, and make sure that all intermediate contexts exist
    @param ctx, the parent JNDI Context under which value will be bound
    @param name, the name relative to ctx where value will be bound
    @param value, the value to bind.
    */
    public static void bind(Context ctx, String name, Object value)
        throws NamingException
    {
        Name n = ctx.getNameParser("").parse(name);
        bind(ctx, n, value);
    }
    /** Bind val to name in ctx, and make sure that all intermediate contexts exist
    @param ctx, the parent JNDI Context under which value will be bound
    @param name, the name relative to ctx where value will be bound
    @param value, the value to bind.
    */
    public static void bind(Context ctx, Name name, Object value)
        throws NamingException
    {
        int size = name.size();
        String atom = name.get(size-1);
        Context parentCtx = createSubcontext(ctx, name.getPrefix(size-1));
        parentCtx.bind(atom, value);
    }

    /** Rebind val to name in ctx, and make sure that all intermediate contexts exist
    @param ctx, the parent JNDI Context under which value will be bound
    @param name, the name relative to ctx where value will be bound
    @param value, the value to bind.
    */
    public static void rebind(Context ctx, String name, Object value)
        throws NamingException
    {
        Name n = ctx.getNameParser("").parse(name);
        rebind(ctx, n, value);
    }
    /** Rebind val to name in ctx, and make sure that all intermediate contexts exist
    @param ctx, the parent JNDI Context under which value will be bound
    @param name, the name relative to ctx where value will be bound
    @param value, the value to bind.
    */
    public static void rebind(Context ctx, Name name, Object value)
        throws NamingException
    {
        int size = name.size();
        String atom = name.get(size-1);
        Context parentCtx = createSubcontext(ctx, name.getPrefix(size-1));
        parentCtx.rebind(atom, value);
    }

    /** Unbinds a name from ctx, and removes parents if they are empty
     @param ctx, the parent JNDI Context under which the name will be unbound
     @param name, The name to unbind
     */
    public static void unbind(Context ctx, String name)
        throws NamingException
    {
        unbind(ctx,ctx.getNameParser("").parse(name));
    }
    
    /** Unbinds a name from ctx, and removes parents if they are empty
     @param ctx, the parent JNDI Context under which the name will be unbound
     @param name, The name to unbind
     */
    public static void unbind(Context ctx, Name name)
       throws NamingException
    {
       ctx.unbind(name); //unbind the end node in the name
       int sz = name.size();
       //walk the list backwards, stopping at the domain since I don't know if
       //a domain can be unbound this way.
       while (--sz > 0) //walk the tree backwards, stopping at the domain
       {
          Name pname = name.getPrefix(sz);
          if (ctx.listBindings(pname).hasMore()) //if we have more children stop now
             break;
          else
             ctx.unbind(pname); //must be no children, nuke it and continue
       }
    }
}
