package org.jboss.test.security.proxy;

import java.io.IOException;
import org.jboss.test.security.interfaces.ReadAccessException;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class SessionSecurityProxy
{

   public String retryableRead(String path) throws IOException
   {
      if( path.startsWith("/restricted") )
         throw new ReadAccessException("/restricted/* read access not allowed");
      return null;
   }

   public String read(String path) throws IOException
   {
      if( path.startsWith("/restricted") )
         throw new SecurityException("/restricted/* read access not allowed");
      return null;
   }

   public void write(String path) throws IOException
   {
      if( path.startsWith("/restricted") )
         throw new SecurityException("/restricted/* write access not allowed");
   }
}
