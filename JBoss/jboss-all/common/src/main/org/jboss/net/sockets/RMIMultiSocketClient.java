package org.jboss.net.sockets;

   
import java.lang.reflect.Method;
import java.lang.reflect.InvocationHandler;
import java.io.Serializable;
import java.util.Random;
import java.rmi.Remote;

/**
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class RMIMultiSocketClient implements InvocationHandler, Serializable
{
   protected Remote[] stubs;
   protected Random random;
   public RMIMultiSocketClient(Remote[] stubs)
   {
      this.stubs = stubs;
      random = new Random();
   }

   public Object invoke(Object proxy, Method method, Object[] args) throws Throwable
   {
      if (method.getName().equals("hashCode"))
      {
         return new Integer(stubs[0].hashCode());
      }
      if (method.getName().equals("equals"))
      {
         return new Boolean(stubs[0].equals(args[0]));
      }
      int i = random.nextInt(stubs.length);
      long hash = MethodHash.calculateHash(method);
      RMIMultiSocket target = (RMIMultiSocket)stubs[i];
      return target.invoke(hash, args);
   }   
}
