package org.jboss.net.sockets;

import java.rmi.server.UnicastRemoteObject;
import java.rmi.Remote;
import java.lang.reflect.Proxy;
import java.rmi.server.RMIClientSocketFactory;
import java.rmi.server.RMIServerSocketFactory;
import java.rmi.RemoteException;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.rmi.NoSuchObjectException;
/**
 *
 * @author bill@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class RMIMultiSocketServer
{
   private static HashMap handlermap = new HashMap();
   private static HashMap stubmap = new HashMap();

   public static Remote exportObject(Remote obj,
                                     int port,
                                     RMIClientSocketFactory csf, 
                                     RMIServerSocketFactory ssf,
                                     Class[] interfaces,
                                     int numSockets)
      throws RemoteException
   {
      Remote[] stubs = new Remote[numSockets];

      Method[] methods = obj.getClass().getMethods();
      
      HashMap invokerMap = new HashMap();
      for (int i = 0; i < methods.length; i++) {
         Long methodkey = new Long(MethodHash.calculateHash(methods[i]));
         invokerMap.put(methodkey, methods[i]);
      }
      
      RMIMultiSocketHandler[] handlers = new RMIMultiSocketHandler[numSockets];
      for (int i = 0; i < numSockets; i++)
      {
         int theport = (port == 0) ? 0 : port + i;
         handlers[i] = new RMIMultiSocketHandler(obj, invokerMap);
         stubs[i] = UnicastRemoteObject.exportObject(handlers[i], theport, csf, ssf);
      }

      Remote remote = (Remote)Proxy.newProxyInstance(
         obj.getClass().getClassLoader(),
         interfaces,
         new RMIMultiSocketClient(stubs));
      stubmap.put(remote, stubs);
      handlermap.put(remote, handlers);
      return remote;
   }

   public static Remote exportObject(Remote obj,
                                     int port,
                                     RMIClientSocketFactory csf, 
                                     RMIServerSocketFactory ssf,
                                     int numSockets)
      throws RemoteException
   {
      return exportObject(obj, port, csf, ssf, obj.getClass().getInterfaces(), numSockets);
   }

   public static boolean unexportObject(Remote obj, boolean force)
      throws NoSuchObjectException
   {
      handlermap.remove(obj);
      Remote[] stubs = (Remote[])stubmap.remove(obj);
      for (int i = 0; i < stubs.length; i++)
      {
         UnicastRemoteObject.unexportObject(stubs[i], force);
      }
      
      return true;
   }
}
