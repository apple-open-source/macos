/*
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jnp.server;

import java.io.ObjectStreamException;
import java.io.NotSerializableException;
import java.rmi.RemoteException;
import java.rmi.server.UnicastRemoteObject;
import java.rmi.server.RemoteObject;
import java.util.Enumeration;
import java.util.Collection;
import java.util.Iterator;
import java.util.Hashtable;
import java.util.Vector;

import javax.naming.Context;
import javax.naming.Name;
import javax.naming.NameClassPair;
import javax.naming.Binding;
import javax.naming.Reference;
import javax.naming.NamingEnumeration;
import javax.naming.InvalidNameException;
import javax.naming.NamingException;
import javax.naming.InvalidNameException;
import javax.naming.NameNotFoundException;
import javax.naming.NotContextException;
import javax.naming.NameAlreadyBoundException;
import javax.naming.CannotProceedException;
import javax.naming.spi.ResolveResult;

import org.jnp.interfaces.*;

/**
 *   <description> 
 *      
 *   @see <related>
 *   @author $Author: patriot1burke $
 *   @version $Revision: 1.9 $
 */
public class NamingServer
   implements Naming, java.io.Serializable
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
   
   protected Hashtable table = new Hashtable();
   protected Name prefix;
   protected NamingParser parser = new NamingParser();
   protected NamingServer parent;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   public NamingServer()
      throws NamingException
   {
      this(null, null);
   }
   
   public NamingServer(Name prefix, NamingServer parent)
      throws NamingException
   {
      if (prefix == null) prefix = parser.parse("");
      this.prefix = prefix;
      
      this.parent = parent;
   }
   
   // Public --------------------------------------------------------

   // Naming implementation -----------------------------------------
   public synchronized void bind(Name name, Object obj, String className)
      throws NamingException
   {
      if (name.isEmpty())
      {
         // Empty names are not allowed
         throw new InvalidNameException();
      } else if (name.size() > 1) 
      {
         // Recurse to find correct context
//         System.out.println("bind#"+name+"#");
         
         Object ctx = getObject(name);
         if (ctx != null)
         {
            if (ctx instanceof NamingServer)
            {
               ((NamingServer)ctx).bind(name.getSuffix(1),obj, className);
            } else if (ctx instanceof Reference)
            {
               // Federation
               if (((Reference)ctx).get("nns") != null)
               {
                  CannotProceedException cpe = new CannotProceedException();
                  cpe.setResolvedObj(ctx);
                  cpe.setRemainingName(name.getSuffix(1));
                  throw cpe;
               } else
               {
                  throw new NotContextException();
               }
            } else
            {
               throw new NotContextException();
            }
         } else
         {
            throw new NameNotFoundException();
         }
      } else
      {
         // Bind object
         if (name.get(0).equals(""))
         {
            throw new InvalidNameException();
         } else
         {
//            System.out.println("bind "+name+"="+obj);
            try
            {
               getBinding(name);
               // Already bound
               throw new NameAlreadyBoundException();
            } catch (NameNotFoundException e)
            {
               setBinding(name,obj,className);
            }
         }
      }
   }

   public synchronized void rebind(Name name, Object obj, String className)
      throws NamingException
   {
      if (name.isEmpty())
      {
         // Empty names are not allowed
         throw new InvalidNameException();
      } else if (name.size() > 1) 
      {
         // Recurse to find correct context
//         System.out.println("rebind#"+name+"#");
         
         Object ctx = getObject(name);
         if (ctx instanceof NamingServer)
         {
            ((NamingServer)ctx).rebind(name.getSuffix(1),obj, className);
         } else if (ctx instanceof Reference)
         {
            // Federation
            if (((Reference)ctx).get("nns") != null)
            {
               CannotProceedException cpe = new CannotProceedException();
               cpe.setResolvedObj(ctx);
               cpe.setRemainingName(name.getSuffix(1));
               throw cpe;
            } else
            {
               throw new NotContextException();
            }
         } else
         {
            throw new NotContextException();
         }
      } else
      {
         // Bind object
         if (name.get(0).equals(""))
         {
            throw new InvalidNameException();
         } else
         {
//            System.out.println("rebind "+name+"="+obj+"("+this+")");
            setBinding(name,obj,className);
         }
      }
   }
   
   public synchronized void unbind(Name name)
      throws NamingException
   {
      if (name.isEmpty())
      {
         // Empty names are not allowed
         throw new InvalidNameException();
      } else if (name.size() > 1) 
      {
         // Recurse to find correct context
//         System.out.println("unbind#"+name+"#");
         
         Object ctx = getObject(name);
         if (ctx instanceof NamingServer)
         {
            ((NamingServer)ctx).unbind(name.getSuffix(1));
         } else if (ctx instanceof Reference)
         {
            // Federation
            if (((Reference)ctx).get("nns") != null)
            {
               CannotProceedException cpe = new CannotProceedException();
               cpe.setResolvedObj(ctx);
               cpe.setRemainingName(name.getSuffix(1));
               throw cpe;
            } else
            {
               throw new NotContextException();
            }
         } else
         {
            throw new NotContextException();
         }
      } else
      {
         // Unbind object
         if (name.get(0).equals(""))
         {
            throw new InvalidNameException();
         } else
         {
//            System.out.println("unbind "+name+"="+getBinding(name));
            if (getBinding(name) != null)
            {
               removeBinding(name);
            } else
            {
               throw new NameNotFoundException();
            }
         }
      }
   }

//   public synchronized Object lookup(Name name)
   public Object lookup(Name name)
      throws NamingException
   {
		Object result;
      if (name.isEmpty())
      {
         // Return this
         result = new NamingContext(null, (Name)(prefix.clone()), getRoot());
      } else if (name.size() > 1)
      {
         // Recurse to find correct context
//         System.out.println("lookup#"+name+"#");
         
         Object ctx = getObject(name);
         if (ctx instanceof NamingServer)
         {
            result = ((NamingServer)ctx).lookup(name.getSuffix(1));
         } else if (ctx instanceof Reference)
         {
            // Federation
            if (((Reference)ctx).get("nns") != null)
            {
               CannotProceedException cpe = new CannotProceedException();
               cpe.setResolvedObj(ctx);
               cpe.setRemainingName(name.getSuffix(1));
               throw cpe;
            }
            
            result = new ResolveResult(ctx, name.getSuffix(1));
         } else
         {
            throw new NotContextException();
         }
      } else
      {
         // Get object to return
         if (name.get(0).equals(""))
         {
            result = new NamingContext(null, prefix, getRoot());
         } else
         {
//            System.out.println("lookup "+name);
            Object res = getObject(name);
            
            if (res instanceof NamingServer)
            {
               Name fullName = (Name)(prefix.clone());
               fullName.addAll(name);
               result = new NamingContext(null, fullName, getRoot());
            }
            else
               result = res;
         }
      }
		
		return result;
   }
   
   public Collection list(Name name)
      throws NamingException
   {
//      System.out.println("list of #"+name+"#"+name.size());
      if (name.isEmpty())
      {
//         System.out.println("list "+name);
         
         Vector list = new Vector();
         Enumeration keys = table.keys();
         while (keys.hasMoreElements())
         {
            String key = (String)keys.nextElement();
            Binding b = getBinding(key);
            
            list.addElement(new NameClassPair(b.getName(),b.getClassName(),true));
         }
         return list;
      } else
      {
//         System.out.println("list#"+name+"#");
         
         Object ctx = getObject(name);
         if (ctx instanceof NamingServer)
         {
            return ((NamingServer)ctx).list(name.getSuffix(1));
         } else if (ctx instanceof Reference)
         {
            // Federation
            if (((Reference)ctx).get("nns") != null)
            {
               CannotProceedException cpe = new CannotProceedException();
               cpe.setResolvedObj(ctx);
               cpe.setRemainingName(name.getSuffix(1));
               throw cpe;
            } else
            {
               throw new NotContextException();
            }
         } else
         {
            throw new NotContextException();
         }
      } 
   }
    
   public Collection listBindings(Name name)
      throws NamingException
   {
      if (name.isEmpty())
      {
         Collection bindings = table.values();
         Collection newBindings = new Vector(bindings.size());
         Iterator enum = bindings.iterator();
         while (enum.hasNext())
         {
            Binding b = (Binding)enum.next();
            if (b.getObject() instanceof NamingServer)
            {
               Name n = (Name)prefix.clone();
               n.add(b.getName());
               newBindings.add(new Binding(b.getName(), 
                                           b.getClassName(),
                                           new NamingContext(null, n, getRoot())));
            } else
            {
               newBindings.add(b);
            }
         }
         
         return newBindings;
      } else
      {
         Object ctx = getObject(name);
         if (ctx instanceof NamingServer)
         {
            return ((NamingServer)ctx).listBindings(name.getSuffix(1));
         } else if (ctx instanceof Reference)
         {
            // Federation
            if (((Reference)ctx).get("nns") != null)
            {
               CannotProceedException cpe = new CannotProceedException();
               cpe.setResolvedObj(ctx);
               cpe.setRemainingName(name.getSuffix(1));
               throw cpe;
            } else
            {
               throw new NotContextException();
            }
         } else
         {
            throw new NotContextException();
         }
      } 
   }
   
   public Context createSubcontext(Name name)
      throws NamingException
   {
       if( name.size() == 0 )
          throw new InvalidNameException("Cannot pass an empty name to createSubcontext");

      NamingException ex = null;
      Context subCtx = null;
      if (name.size() > 1)
      {         
         Object ctx = getObject(name);
         if (ctx != null)
         {
            Name subCtxName = name.getSuffix(1);
            if (ctx instanceof NamingServer)
            {
               subCtx = ((NamingServer)ctx).createSubcontext(subCtxName);
            }
            else if (ctx instanceof Reference)
            {
               // Federation
               if (((Reference)ctx).get("nns") != null)
               {
                  CannotProceedException cpe = new CannotProceedException();
                  cpe.setResolvedObj(ctx);
                  cpe.setRemainingName(subCtxName);
                  throw cpe;
               }
               else
               {
                  ex = new NotContextException();
                  ex.setResolvedName(name.getPrefix(0));
                  ex.setRemainingName(subCtxName);
                  throw ex;
               }
            }
            else
            {
               ex = new NotContextException();
               ex.setResolvedName(name.getPrefix(0));
               ex.setRemainingName(subCtxName);
               throw ex;
            }
         }
         else
         {
            ex = new NameNotFoundException();
            ex.setRemainingName(name);
            throw ex;
         }
      }
      else
      {
         Object binding = table.get(name.get(0));
         if( binding != null )
         {
            ex = new NameAlreadyBoundException();
            ex.setResolvedName(prefix);
            ex.setRemainingName(name);
            throw ex;
         }
         else
         {
            Name fullName = (Name) prefix.clone();
            fullName.addAll(name);
            NamingServer subContext = new NamingServer(fullName, this);
            setBinding(name, subContext, NamingContext.class.getName());
            subCtx = new NamingContext(null, fullName, getRoot());
         }
      }
      return subCtx;
   }
      
   public Naming getRoot()
   {
      if (parent == null)
         return this;
      else
         return parent.getRoot();
   }

   // Y overrides ---------------------------------------------------

   // Package protected ---------------------------------------------
    
   // Protected -----------------------------------------------------
    
   // Private -------------------------------------------------------
   private void setBinding(Name name, Object obj, String className)
   {
      String n = name.toString();
      table.put(n, new Binding(n, className, obj, true));
   }

   private Binding getBinding(String key)
      throws NameNotFoundException
   {
      Binding b = (Binding)table.get(key);
      if (b == null)
      {
         throw new NameNotFoundException(key + " not bound");
      }
      return b;
   }

   private Binding getBinding(Name key)
      throws NameNotFoundException
   {
      return getBinding(key.get(0));
   }
   
   private Object getObject(Name key)
      throws NameNotFoundException
   {
      return getBinding(key).getObject();
   }

   private void removeBinding(Name name)
   {
      table.remove(name.get(0));
   }
   
   private NamingServer getParent()
   {
      return parent;
   }
   // Inner classes -------------------------------------------------
}
