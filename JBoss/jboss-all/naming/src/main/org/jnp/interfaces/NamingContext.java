/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jnp.interfaces;

import java.io.BufferedInputStream;
import java.io.ObjectInputStream;
import java.io.IOException;
import java.lang.ref.WeakReference;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.net.DatagramPacket;
import java.net.InetAddress;
import java.net.MulticastSocket;
import java.net.Socket;
import java.rmi.ConnectException;
import java.rmi.MarshalledObject;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.HashMap;
import java.util.Iterator;
import java.util.StringTokenizer;

import javax.naming.Binding;
import javax.naming.CannotProceedException;
import javax.naming.CommunicationException;
import javax.naming.ConfigurationException;
import javax.naming.Context;
import javax.naming.InvalidNameException;
import javax.naming.InitialContext;
import javax.naming.LinkRef;
import javax.naming.Name;
import javax.naming.NamingEnumeration;
import javax.naming.NamingException;
import javax.naming.NameParser;
import javax.naming.NotContextException;
import javax.naming.OperationNotSupportedException;
import javax.naming.Reference;
import javax.naming.Referenceable;
import javax.naming.ServiceUnavailableException;
import javax.naming.spi.NamingManager;
import javax.naming.spi.ResolveResult;
import javax.net.SocketFactory;

import org.jboss.logging.Logger;

/** This class provides the jnp provider Context implementation. It is a
 * Context interface wrapper for a RMI Naming instance that is obtained from
 * either the local server instance or by locating the server given by the
 * Context.PROVIDER_URL value.
 *
 * This class also serves as the jnp url resolution context. jnp style urls
 * passed to the
 *
 *   @author oberg
 *   @author scott.stark@jboss.org
 *   @version $Revision: 1.18.2.15 $
 *
 * <p><b>Revisions:</b><br>
 * <p><b>2001/09/14: billb</b>
 * <ol>
 *   <li> Provider URL can now be a commented delimited list. Will loop through list until a connection is made.
 * </ol>
 */
public class NamingContext
   implements Context, java.io.Serializable
{
   // Constants -----------------------------------------------------
   /** @since 1.7 */
   static final long serialVersionUID = 8906455608484282128L;
   /** The javax.net.SocketFactory impl to use for the bootstrap socket */
   public static final String JNP_SOCKET_FACTORY = "jnp.socketFactory";
   /** The local address to bind the connected bootstrap socket to */
   public static final String JNP_LOCAL_ADDRESS = "jnp.localAddress";
   /** The local port to bind the connected bootstrap socket to */
   public static final String JNP_LOCAL_PORT = "jnp.localPort";
   /** A flag to disable the broadcast discovery queries */
   public static final String JNP_DISABLE_DISCOVERY = "jnp.disableDiscovery";
   /** The cluster partition discovery should be restricted to */
   public static final String JNP_PARTITION_NAME = "jnp.partitionName";
   /** The multicast IP/address to which the discovery query is sent */
   public static final String JNP_DISCOVERY_GROUP = "jnp.discoveryGroup";
   /** The port to which the discovery query is sent */
   public static final String JNP_DISCOVERY_PORT = "jnp.discoveryPort";
   /** The time in MS to wait for a discovery query response */
   public static final String JNP_DISCOVERY_TIMEOUT = "jnp.discoveryTimeout";

   /** The default discovery multicast information */
   public final static String DEFAULT_DISCOVERY_GROUP_ADDRESS = "230.0.0.4";
   public final static int DEFAULT_DISCOVERY_GROUP_PORT = 1102;
   public final static int DEFAULT_DISCOVERY_TIMEOUT = 5000;

   /** Maximum number of retries on a ConnectException */
   public static int MAX_RETRIES = 10;
   /** The JBoss logging interface */
   private static Logger log = Logger.getLogger(NamingContext.class);

   // Static --------------------------------------------------------
   
   public static Hashtable haServers = new Hashtable ();
   
   public static void setHANamingServerForPartition (String partitionName, Naming haServer)
   {
      haServers.put (partitionName, haServer);
   }

   public static void removeHANamingServerForPartition (String partitionName)
   {
      haServers.remove (partitionName);
   }

   public static Naming getHANamingServerForPartition (String partitionName)
   {
      return (Naming)haServers.get (partitionName);
   }

   public static Naming localServer;

   // Attributes ----------------------------------------------------
   Naming naming;
   Hashtable env;
   Name prefix;

   NameParser parser = new NamingParser ();
   
   // Static --------------------------------------------------------
   
   // Cache of naming server stubs
   // This is a critical optimization in the case where new InitialContext
   // is performed often. The server stub will be shared between all those
   // calls, which will improve performance.
   // Weak references are used so if no contexts use a particular server
   // it will be removed from the cache.
   static HashMap cachedServers = new HashMap ();
   
   static void addServer (String name, Naming server)
   {
      // Add server to map
      // Clone and synchronize to minimize delay for readers of the map
      synchronized (NamingContext.class)
      {
         HashMap newServers = (HashMap)cachedServers.clone ();
         newServers.put (name, new WeakReference (server));
         cachedServers = newServers;
      }
   }
   
   static Naming getServer(String host, int port, Hashtable serverEnv)
      throws NamingException
   {
      // Check the server cache for a host:port entry
      String hostKey = host+":"+port;
      WeakReference ref = (WeakReference)cachedServers.get(hostKey);
      Naming server;
      if (ref != null)
      {
         server = (Naming) ref.get();
         if (server != null)
         {
            return server;
         }
      }

      // Server not found; add it to cache
      try
      {
         SocketFactory factory = loadSocketFactory(serverEnv);
         Socket s;

         try
         {
            InetAddress localAddr = null;
            int localPort = 0;
            String localAddrStr = (String) serverEnv.get(JNP_LOCAL_ADDRESS);
            String localPortStr = (String) serverEnv.get(JNP_LOCAL_PORT);
            if( localAddrStr != null )
               localAddr = InetAddress.getByName(localAddrStr);
            if( localPortStr != null )
               localPort = Integer.parseInt(localPortStr);
            s = factory.createSocket(host, port, localAddr, localPort);
         }
         catch (IOException e)
         {
            NamingException ex = new ServiceUnavailableException("Failed to connect to server "+hostKey);
            ex.setRootCause(e);
            throw ex;
         }

         // Get stub from naming server
         BufferedInputStream bis = new BufferedInputStream(s.getInputStream());
         ObjectInputStream in = new ObjectInputStream(bis);
         MarshalledObject stub = (MarshalledObject) in.readObject();
         server = (Naming) stub.get();
         s.close();

         // Add it to cache
         addServer(hostKey, server);

         return server;
      }
      catch(IOException e)
      {
         NamingException ex = new CommunicationException("Failed to retrieve stub from server "+hostKey);
         ex.setRootCause(e);
         throw ex;
      }
      catch(Exception e)
      {
         NamingException ex = new CommunicationException("Failed to connect to server "+hostKey);
         ex.setRootCause (e);
         throw ex;
      }
   }

   /** Create a SocketFactory based on the JNP_SOCKET_FACTORY property in the
    given env. If JNP_SOCKET_FACTORY is not specified default to the
    TimedSocketFactory.
    */
   static SocketFactory loadSocketFactory(Hashtable serverEnv)
      throws ClassNotFoundException, IllegalAccessException,
      InstantiationException, InvocationTargetException
   {
      SocketFactory factory = null;

      // Get the socket factory classname
      String socketFactoryName = (String) serverEnv.get(JNP_SOCKET_FACTORY);
      if( socketFactoryName == null ||
         socketFactoryName.equals(TimedSocketFactory.class.getName()) )
      {
         factory = new TimedSocketFactory(serverEnv);
         return factory;
      }

      /* Create the socket factory. Look for a ctor that accepts a
       Hashtable and if not found use the default ctor.
       */
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      Class factoryClass = loader.loadClass(socketFactoryName);
      try
      {
         Class[] ctorSig = {Hashtable.class};
         Constructor ctor = factoryClass.getConstructor(ctorSig);
         Object[] ctorArgs = {serverEnv};
         factory = (SocketFactory) ctor.newInstance(ctorArgs);
      }
      catch(NoSuchMethodException e)
      {
         // Use the default ctor
         factory = (SocketFactory) factoryClass.newInstance();
      }
      return factory;
   }

   static void removeServer (Hashtable serverEnv)
   {
      String host = "localhost";
      int port = 1099;
      
      // Locate naming service
      if (serverEnv.get (Context.PROVIDER_URL) != null)
      {
         String providerURL = (String)serverEnv.get(Context.PROVIDER_URL);

         StringTokenizer tokenizer = new StringTokenizer (providerURL, ",");
         while (tokenizer.hasMoreElements ())
         {
            String url = tokenizer.nextToken ();

            try
            {
               // Parse the url into a host:port form, stripping any protocol
               Name urlAsName = new NamingParser ().parse(url);
               String server = parseNameForScheme(urlAsName);
               if( server != null )
                  url = server;
               int colon = url.indexOf (':');
               if( colon < 0 )
               {
                  host = url;
               }
               else
               {
                  host = url.substring (0, colon).trim();
                  try
                  {
                     port = Integer.parseInt (url.substring (colon+1).trim ());
                  }
                  catch (Exception ex)
                  {
                     // Use default;
                  }
                }

               // Remove server from map
               // Clone and synchronize to minimize delay for readers of the map
               synchronized (NamingContext.class)
               {
                  HashMap newServers = (HashMap)cachedServers.clone ();
                  newServers.remove (host+":"+port);
                  cachedServers = newServers;
               }

            }
            catch (NamingException ignored) {}


         }
      }
      else
      {
         // Don't do anything for local server
      }
      
   }
   
   /** Called to remove any url scheme atoms and extract the naming
    * service hostname:port information.
    * @param n the name component to the parsed. After returning n will
    * have all scheme related atoms removed.
    * @return the naming service hostname:port information string if name
    * contained the host information.
    */
   static String parseNameForScheme (Name n) throws InvalidNameException
   {
      String serverInfo = null;
      if( n.size () > 0 )
      {
         String scheme = n.get (0);
         int schemeLength = 0;
         if( scheme.startsWith ("java:") )
            schemeLength = 5;
         else if( scheme.startsWith ("jnp:") )
            schemeLength = 4;
         else if( scheme.startsWith ("jnps:") )
            schemeLength = 5;
         else if( scheme.startsWith ("jnp-http:") )
            schemeLength = 9;
         else if( scheme.startsWith ("jnp-https:") )
            schemeLength = 10;
         if( schemeLength > 0 )
         {
            String suffix = scheme.substring (schemeLength);
            if( suffix.length () == 0 )
            {
               // Scheme was "url:/..."
               n.remove (0);
               if( n.size () > 1 && n.get (0).equals ("") )
               {
                  // Scheme was "url://hostname:port/..."
                  // Get hostname:port value for the naming server
                  serverInfo = n.get (1);
                  n.remove (0);
                  n.remove (0);
                  // If n is a empty atom remove it or else a '/' will result
                  if( n.size () == 1 && n.get (0).length () == 0 )
                     n.remove (0);
               }
            }
            else
            {
               // Scheme was "url:foo" -> reinsert "foo"
               n.remove (0);
               n.add (0, suffix);
            }
         }
      }
      return serverInfo;
   }
   
   public static void setLocal (Naming server)
   {
      localServer = server;
   }
   
   // Constructors --------------------------------------------------
   public NamingContext (Hashtable e, Name baseName, Naming server)
      throws NamingException
   {
      if (baseName == null)
         this.prefix = parser.parse ("");
      else
         this.prefix = baseName;
      
      if (e != null)
         this.env = (Hashtable)e.clone ();
      else
         this.env = new Hashtable ();
      
      this.naming = server;
   }
   
   // Public --------------------------------------------------------
   public Naming getNaming()
   {
      return this.naming;
   }
   public void setNaming(Naming server)
   {
      this.naming = server;
   }

   // Context implementation ----------------------------------------
   public void rebind (String name, Object obj)
      throws NamingException
   {
      rebind (getNameParser (name).parse (name), obj);
   }
   
   public void rebind (Name name, Object obj)
      throws NamingException
   {
      Hashtable refEnv = getEnv (name);
      checkRef (refEnv);

      try
      {
         String className;
         
         // Referenceable
         if (obj instanceof Referenceable)
            obj = ((Referenceable)obj).getReference ();
         
         if (!(obj instanceof Reference))
         {
            className = obj.getClass ().getName ();
            // Normal object - serialize using a MarshalledValuePair
            obj = new MarshalledValuePair(obj);
         }
         else
         {
            className = ((Reference)obj).getClassName ();
         }
         naming.rebind (getAbsoluteName (name),obj, className);
      }
      catch (CannotProceedException cpe)
      {
         cpe.setEnvironment (refEnv);
         Context cctx = NamingManager.getContinuationContext (cpe);
         cctx.rebind (cpe.getRemainingName (), obj);
      } catch (IOException e)
      {
         naming = null;
         removeServer (refEnv);
         NamingException ex = new CommunicationException ();
         ex.setRootCause (e);
         throw ex;
      }
   }
   
   public void bind (String name, Object obj)
   throws NamingException
   {
      bind (getNameParser (name).parse (name), obj);
   }
   
   public void bind (Name name, Object obj)
      throws NamingException
   {
      Hashtable refEnv = getEnv (name);
      checkRef (refEnv);
      
      try
      {
         String className;
         
         // Referenceable
         if (obj instanceof Referenceable)
            obj = ((Referenceable)obj).getReference ();
         
         if (!(obj instanceof Reference))
         {
            className = obj.getClass ().getName ();
            
            // Normal object - serialize using a MarshalledValuePair
            obj = new MarshalledValuePair(obj);
         }
         else
         {
            className = ((Reference)obj).getClassName ();
         }
         name = getAbsoluteName (name);
         naming.bind (name,obj, className);
      } catch (CannotProceedException cpe)
      {
         cpe.setEnvironment (refEnv);
         Context cctx = NamingManager.getContinuationContext (cpe);
         cctx.bind (cpe.getRemainingName (), obj);
      } catch (IOException e)
      {
         naming = null;
         removeServer (refEnv);
         NamingException ex = new CommunicationException ();
         ex.setRootCause (e);
         throw ex;
      }
   }
   
   public Object lookup (String name)
   throws NamingException
   {
      return lookup (getNameParser (name).parse (name));
   }
   
   public Object lookup (Name name)
      throws NamingException
   {
      Hashtable refEnv = getEnv (name);
      checkRef (refEnv);
      
      // Empty?
      if (name.isEmpty ())
         return new NamingContext (refEnv, prefix, naming);
      
      try
      {
         Name n = getAbsoluteName (name);
         Object res = null;
         for (int i = 0; i < MAX_RETRIES; i++)
         {
            try
            {
               res = naming.lookup (n);
               break;
            }
            catch (ConnectException ce)
            {
               // We may overload server so sleep and retry
               if (i + 1 < MAX_RETRIES) 
               {
                  try
                  {
                     Thread.sleep(1);
                  }
                  catch (InterruptedException ignored) {}
                  continue;
               }
               // Throw the exception to flush the bad server
               throw ce;
            }
         }
         if (res instanceof MarshalledValuePair)
         {
            MarshalledValuePair mvp = (MarshalledValuePair) res;
            return mvp.get();
         }
         else if(res instanceof MarshalledObject)
         {
            MarshalledObject mo = (MarshalledObject) res;
            return mo.get();
         }
         else if (res instanceof Context)
         {
            // Add env
            Enumeration keys = refEnv.keys();
            while (keys.hasMoreElements())
            {
               String key = (String)keys.nextElement ();
               ((Context)res).addToEnvironment(key, refEnv.get (key));
            }
            return res;
         }
         else if (res instanceof ResolveResult)
         {
            // Dereference partial result
            try
            {
               Object resolveRes = ((ResolveResult)res).getResolvedObj ();
               if (resolveRes instanceof LinkRef)
               {
                  String ref = ((LinkRef)resolveRes).getLinkName ();
                  Context ctx;
                  try
                  {
                     ctx = (Context) resolveLink(resolveRes, null);
                     return ctx.lookup (((ResolveResult)res).getRemainingName ());
                  } catch (ClassCastException e)
                  {
                     throw new NotContextException (ref + " is not a context");
                  }
               } else
               {
                  try
                  {
                     Context ctx = (Context)NamingManager.getObjectInstance (resolveRes,
                     getAbsoluteName (name),
                     this,
                     refEnv);
                     return ctx.lookup (((ResolveResult)res).getRemainingName ());
                  } catch (ClassCastException e)
                  {
                     throw new NotContextException ();
                  }
               }
            } catch (NamingException e)
            {
               throw e;
            } catch (Exception e)
            {
               NamingException ex = new NamingException ("Could not dereference object");
               ex.setRootCause (e);
               throw ex;
            }
         }
         else if (res instanceof LinkRef)
         {
            // Dereference link
            res = resolveLink(res, refEnv);
         }
         else if (res instanceof Reference)
         {
            // Dereference object
            try
            {
               res = NamingManager.getObjectInstance (res,
               getAbsoluteName (name),
               this,
               refEnv);
               if( res instanceof LinkRef )
                  res = resolveLink(res, refEnv);
            } catch (NamingException e)
            {
               throw e;
            } catch (Exception e)
            {
               NamingException ex = new NamingException ("Could not dereference object");
               ex.setRootCause (e);
               throw ex;
            }
         }
         
         return res;
      } catch (CannotProceedException cpe)
      {
         cpe.setEnvironment (refEnv);
         Context cctx = NamingManager.getContinuationContext (cpe);
         return cctx.lookup (cpe.getRemainingName ());
      } catch (IOException e)
      {
         naming = null;
         removeServer (refEnv);
         NamingException ex = new CommunicationException ();
         ex.setRootCause (e);
         throw ex;
      } catch (ClassNotFoundException e)
      {
         NamingException ex = new CommunicationException ();
         ex.setRootCause (e);
         throw ex;
      }
   }
   
   public void unbind (String name)
   throws NamingException
   {
      unbind (getNameParser (name).parse (name));
   }
   
   
   public void unbind (Name name)
   throws NamingException
   {
      Hashtable refEnv = getEnv (name);
      checkRef (refEnv);
      
      try
      {
         naming.unbind (getAbsoluteName (name));
      } catch (CannotProceedException cpe)
      {
         cpe.setEnvironment (refEnv);
         Context cctx = NamingManager.getContinuationContext (cpe);
         cctx.unbind (cpe.getRemainingName ());
      } catch (IOException e)
      {
         naming = null;
         removeServer (refEnv);
         NamingException ex = new CommunicationException ();
         ex.setRootCause (e);
         throw ex;
      }
   }
   
   public void rename (String oldname, String newname)
   throws NamingException
   {
      rename (getNameParser (oldname).parse (oldname), getNameParser (newname).parse (newname));
   }
   
   public void rename (Name oldName, Name newName)
   throws NamingException
   {
      bind (newName,lookup (oldName));
      unbind (oldName);
   }
   
   public NamingEnumeration list (String name)
   throws NamingException
   {
      return list (getNameParser (name).parse (name));
   }
   
   public NamingEnumeration list (Name name)
   throws NamingException
   {
      Hashtable refEnv = getEnv (name);
      checkRef (refEnv);
      
      try
      {
         return new NamingEnumerationImpl (naming.list (getAbsoluteName (name)));
      } catch (CannotProceedException cpe)
      {
         cpe.setEnvironment (refEnv);
         Context cctx = NamingManager.getContinuationContext (cpe);
         return cctx.list (cpe.getRemainingName ());
      } catch (IOException e)
      {
         naming = null;
         removeServer (refEnv);
         NamingException ex = new CommunicationException ();
         ex.setRootCause (e);
         throw ex;
      }
   }
   
   public NamingEnumeration listBindings (String name)
   throws NamingException
   {
      return listBindings (getNameParser (name).parse (name));
   }
   
   public NamingEnumeration listBindings (Name name)
   throws NamingException
   {
      Hashtable refEnv = getEnv (name);
      checkRef (refEnv);
      
      try
      {
         // Get list
         Collection bindings = naming.listBindings (getAbsoluteName (name));
         Collection realBindings = new ArrayList (bindings.size ());
         
         // Convert marshalled objects
         Iterator enum = bindings.iterator ();
         while (enum.hasNext ())
         {
            Binding binding = (Binding)enum.next ();
            Object obj = binding.getObject ();
            if (obj instanceof MarshalledValuePair)
            {
               try
               {
                  obj = ((MarshalledValuePair)obj).get ();
               }
               catch (ClassNotFoundException e)
               {
                  NamingException ex = new CommunicationException ();
                  ex.setRootCause (e);
                  throw ex;
               }
            }
            else if(obj instanceof MarshalledObject)
            {
               try
               {
                  obj = ((MarshalledObject)obj).get ();
               }
               catch (ClassNotFoundException e)
               {
                  NamingException ex = new CommunicationException ();
                  ex.setRootCause (e);
                  throw ex;
               }
            }
            realBindings.add (new Binding (binding.getName (), binding.getClassName (), obj));
         }
         
         // Return transformed list of bindings
         return new NamingEnumerationImpl (realBindings);
      } catch (CannotProceedException cpe)
      {
         cpe.setEnvironment (refEnv);
         Context cctx = NamingManager.getContinuationContext (cpe);
         return cctx.listBindings (cpe.getRemainingName ());
      } catch (IOException e)
      {
         naming = null;
         removeServer (refEnv);
         NamingException ex = new CommunicationException ();
         ex.setRootCause (e);
         throw ex;
      }
   }
   
   public String composeName (String name, String prefix)
   throws NamingException
   {
      Name result = composeName (parser.parse (name),
      parser.parse (prefix));
      return result.toString ();
   }
   
   public Name composeName (Name name, Name prefix)
   throws NamingException
   {
      Name result = (Name)(prefix.clone ());
      result.addAll (name);
      return result;
   }
   
   public NameParser getNameParser (String name)
   throws NamingException
   {
      return parser;
   }
   
   public NameParser getNameParser (Name name)
   throws NamingException
   {
      return getNameParser (name.toString ());
   }
   
   public Context createSubcontext (String name)
   throws NamingException
   {
      return createSubcontext (getNameParser (name).parse (name));
   }
   
   public Context createSubcontext (Name name)
   throws NamingException
   {
      if( name.size () == 0 )
         throw new InvalidNameException ("Cannot pass an empty name to createSubcontext");
      
      Hashtable refEnv = getEnv (name);
      checkRef (refEnv);
      try
      {
         name = getAbsoluteName (name);
         return naming.createSubcontext (name);
      }
      catch (CannotProceedException cpe)
      {
         cpe.setEnvironment (refEnv);
         Context cctx = NamingManager.getContinuationContext (cpe);
         return cctx.createSubcontext (cpe.getRemainingName ());
      }
      catch (IOException e)
      {
         naming = null;
         removeServer (refEnv);
         NamingException ex = new CommunicationException ();
         ex.setRootCause (e);
         throw ex;
      }
   }
   
   public Object addToEnvironment (String propName, Object propVal)
      throws NamingException
   {
      Object old = env.get (propName);
      env.put (propName,propVal);
      return old;
   }
   
   public Object removeFromEnvironment (String propName)
      throws NamingException
   {
      return env.remove (propName);
   }
   
   public Hashtable getEnvironment ()
      throws NamingException
   {
      return env;
   }
   
   public void close ()
      throws NamingException
   {
      env = null;
      naming = null;
   }
   
   public String getNameInNamespace ()
   throws NamingException
   {
      return prefix.toString ();
   }
   
   public void destroySubcontext (String name)
   throws NamingException
   {
      throw new OperationNotSupportedException ();
   }
   
   public void destroySubcontext (Name name)
   throws NamingException
   {
      throw new OperationNotSupportedException ();
   }
   
   public Object lookupLink (String name)
   throws NamingException
   {
      return lookupLink (getNameParser (name).parse (name));
   }
   
   /** Lookup the object referred to by name but don't dereferrence the final
    * component. This really just involves returning the raw value returned by
    * the Naming.lookup() method.
    * @return the raw object bound under name.
    */
   public Object lookupLink (Name name)
      throws NamingException
   {
      if( name.isEmpty () )
         return lookup (name);
      
      Object link = null;
      try
      {
         Name n = getAbsoluteName (name);
         link = naming.lookup (n);
         if (!(link instanceof LinkRef) && link instanceof Reference)
            link = NamingManager.getObjectInstance (link, n, this, null);
      }
      catch(IOException e)
      {
         naming = null;
         removeServer (env);
         NamingException ex = new CommunicationException ();
         ex.setRootCause (e);
         throw ex;
      }
      catch(Exception e)
      {
         NamingException ex = new NamingException ("Could not lookup link");
         ex.setRemainingName(name);
         ex.setRootCause(e);
         throw ex;
      }
      return link;
   }

   protected Object resolveLink(Object res, Hashtable refEnv)
      throws NamingException
   {
      Object linkResult = null;
      try
      {
         LinkRef link = (LinkRef) res;
         String ref = link.getLinkName();
         if (ref.startsWith ("./"))
            linkResult = lookup (ref.substring (2));
         else if( refEnv != null )
            linkResult = new InitialContext(refEnv).lookup (ref);
         else
            linkResult = new InitialContext().lookup (ref);
      }
      catch (Exception e)
      {
         NamingException ex = new NamingException ("Could not dereference object");
         ex.setRootCause (e);
         throw ex;
      }
      return linkResult;
   }

   // Private -------------------------------------------------------
   
   /** This methods sends a broadcast message on the network and asks and
    * HA-JNDI server to sent it the HA-JNDI stub
    */
   private Naming discoverServer(Hashtable serverEnv) throws NamingException
   {
      boolean trace = log.isTraceEnabled();
      // Check if discovery should be done
      String disableDiscovery = (String) serverEnv.get(JNP_DISABLE_DISCOVERY);
      if( Boolean.valueOf(disableDiscovery) == Boolean.TRUE )
      {
         if( trace )
            log.trace("Skipping discovery due to disable flag");
         return null;
      }
      
      // we first try to discover the server locally
      //
      String partitionName = (String) serverEnv.get(JNP_PARTITION_NAME);
      Naming server = null;
      if (partitionName != null)
      {
         server = getHANamingServerForPartition (partitionName);
         if (server != null)
            return server;
      }
      
      // We next broadcast a HelloWorld datagram (multicast)
      // Any listening server will answer with its IP address:port in another datagram
      // we will then use this to make a standard "lookup"
      //
      MulticastSocket s = null;
      InetAddress iaGroup = null;
      try
      {
         String group = DEFAULT_DISCOVERY_GROUP_ADDRESS;
         int port = DEFAULT_DISCOVERY_GROUP_PORT;
         int timeout = DEFAULT_DISCOVERY_TIMEOUT;

         String discoveryGroup = (String) serverEnv.get(JNP_DISCOVERY_GROUP);
         if( discoveryGroup != null )
            group = discoveryGroup;
         String discoveryTimeout = (String) serverEnv.get(JNP_DISCOVERY_TIMEOUT);
         if( discoveryTimeout == null )
         {
            // Check the old property name
            discoveryTimeout = (String) serverEnv.get("DISCOVERY_TIMEOUT");
         }
         if (discoveryTimeout != null && !discoveryTimeout.equals (""))
            timeout = Integer.parseInt (discoveryTimeout);

         String discoveryGroupPort = (String) serverEnv.get(JNP_DISCOVERY_PORT);
         if( discoveryGroupPort == null )
         {
            // Check the old property name
            discoveryGroupPort = (String) serverEnv.get("DISCOVERY_GROUP");
         }
         if (discoveryGroupPort != null && !discoveryGroupPort.equals (""))
         {
            int colon = discoveryGroupPort.indexOf (':');
            if( colon < 0 )
            {
               // No group given, just the port
               try
               {
                  port = Integer.parseInt (discoveryGroupPort);
               }
               catch (Exception ex)
               {
                  log.warn("Failed to parse port: "+discoveryGroupPort, ex);
               }
            }
            else
            {
               // The old group:port syntax was given
               group = discoveryGroupPort.substring (0, colon);
               String portStr = discoveryGroupPort.substring (colon+1);
               try
               {
                  port = Integer.parseInt (portStr);
               }
               catch (Exception ex)
               {
                  log.warn("Failed to parse port: "+portStr, ex);
               }
            }
         }
         
         iaGroup = InetAddress.getByName (group);
         s = new MulticastSocket (port);
         s.setSoTimeout (timeout);
         s.joinGroup(iaGroup);

         DatagramPacket packet;
         // Send a request optionally restricted to a cluster partition
         StringBuffer data = new StringBuffer("GET_ADDRESS");
         if( partitionName != null )
            data.append(":"+partitionName);
         byte[] buf = data.toString().getBytes();
         packet = new DatagramPacket (buf, buf.length, iaGroup, port);
         if( trace )
            log.trace("Sending discovery packet("+data+") to: "+iaGroup+":"+port);
         s.send (packet);
         // Look for a reply
         // IP address + port number = 128.128.128.128:65535 => (12+3) + 1 + (5) = 21

         buf = new byte[50];
         packet = new DatagramPacket (buf, buf.length);
         s.receive (packet);
         String myServer = new String (packet.getData ()).trim();
         if( trace )
            log.trace("Received answer packet: "+myServer);
         while (myServer != null && myServer.startsWith("GET_ADDRESS"))
         {
            Arrays.fill(buf, (byte) 0);
            s.receive (packet);
            byte[] reply = packet.getData();
            myServer = new String (reply).trim();
            if( trace )
               log.trace("Received answer packet: "+myServer);
         }
         String serverHost;
         int serverPort;

         int colon = myServer.indexOf (':');
         if( colon >= 0 )
         {
            serverHost = myServer.substring (0, colon);
            serverPort = Integer.valueOf (myServer.substring (colon + 1)).intValue ();
            server = getServer (serverHost, serverPort, serverEnv);
         }
         return server;
      }
      catch (IOException e)
      {
         if( trace )
            log.trace("Discovery failed", e);
         NamingException ex = new CommunicationException (e.getMessage ());
         ex.setRootCause (e);
         throw ex;
      }
      finally
      {
         try
         {
            if (s != null)
               s.leaveGroup (iaGroup);
         }
         catch(Exception ignore)
         {
         }
         try
         {
            if (s != null)
               s.close ();
         }
         catch(Exception ignore)
         {
         }
      }
   }

   private void checkRef(Hashtable refEnv)
      throws NamingException
   {
      if (naming == null)
      {
         String host = "localhost";
         int port = 1099;
         
         // Locate naming service
         String urls = (String) refEnv.get (Context.PROVIDER_URL);
         if (urls != null && urls.length () > 0)
         {
            StringTokenizer tokenizer = new StringTokenizer (urls, ",");
            while (tokenizer.hasMoreElements ())
            {
               String url = tokenizer.nextToken ();
               // Parse the url into a host:port form, stripping any protocol
               Name urlAsName = getNameParser("").parse(url);
               String server = parseNameForScheme(urlAsName);
               if( server != null )
                  url = server;
               int colon = url.indexOf (':');
               if( colon < 0 )
               {
                  host = url;
               }
               else
               {
                  host = url.substring (0, colon).trim();
                  try
                  {
                     port = Integer.parseInt (url.substring (colon+1).trim ());
                  }
                  catch (Exception ex)
                  {
                     // Use default;
                  }
               }
               try
               {
                  // Get server from cache
                  naming = getServer (host, port, refEnv);
               }
               catch(Exception e)
               {
                  log.warn("Failed to connect to "+host+":"+port, e);
               }
            }

            // If there is still no 
            if (naming == null)
            {
               naming = discoverServer (refEnv);
               if (naming == null)
                  throw new CommunicationException ("Could not obtain connection to any of these urls: " + urls);
            }
         }
         else
         {
            // If we are in a clustering scenario, the client code may request a context
            // for a *specific* HA-JNDI service (i.e. linked to a *specific* partition)
            // EVEN if the lookup is done inside a JBoss VM. For example, a JBoss service
            // may do a lookup on a HA-JNDI service running on another host *without*
            // explicitly providing a PROVIDER_URL but simply by providing a JNP_PARTITON_NAME
            // parameter so that dynamic discovery can be used
            //
            String jnpPartitionName = (String)refEnv.get (JNP_PARTITION_NAME);
            if (jnpPartitionName != null)
            {
               // the client is requesting for a specific partition name
               // 
               naming = discoverServer (refEnv);
               if (naming == null)
                  throw new ConfigurationException 
                     ("No valid context could be build for jnp.partitionName=" + jnpPartitionName);
            }
            else
            {
               // Use server in same JVM
               naming = localServer;

               if (naming == null)
               {
                  naming = discoverServer (refEnv);
                  if (naming == null)
                     // Local, but no local JNDI provider found!
                     throw new ConfigurationException ("No valid Context.PROVIDER_URL was found");
               }
            }
         }
      }
   }
   
   private Name getAbsoluteName (Name n)
      throws NamingException
   {
      if (n.isEmpty ())
         return composeName (n,prefix);
      else if (n.get (0).toString ().equals ("")) // Absolute name
         return n.getSuffix (1);
      else // Add prefix
         return composeName (n,prefix);
   }
   
   private Hashtable getEnv (Name n)
      throws InvalidNameException
   {
      Hashtable nameEnv = env;
      String serverInfo = parseNameForScheme (n);
      if( serverInfo != null )
      {
         // Set hostname:port value for the naming server
         nameEnv = (Hashtable)env.clone ();
         nameEnv.put (Context.PROVIDER_URL, serverInfo);
      }
      return nameEnv;
   }

   // Inner classes -------------------------------------------------
}
