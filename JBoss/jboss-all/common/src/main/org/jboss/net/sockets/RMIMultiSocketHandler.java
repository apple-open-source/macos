package org.jboss.net.sockets;

import java.lang.reflect.Method;
import java.util.Map;
/**
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class RMIMultiSocketHandler implements RMIMultiSocket
{
   Object target;
   Map invokerMap;
   public RMIMultiSocketHandler(Object target, Map invokerMap)
   {
      this.target = target;
      this.invokerMap = invokerMap;
   }

   public Object invoke (long methodHash, Object[] args) throws Exception
   {
      Method method = (Method)invokerMap.get(new Long(methodHash));
      return method.invoke(target, args);
   }
}
