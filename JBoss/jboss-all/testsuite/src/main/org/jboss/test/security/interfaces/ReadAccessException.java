package org.jboss.test.security.interfaces;

import java.io.IOException;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class ReadAccessException extends IOException
{
   public ReadAccessException(String s)
   {
      super(s);
   }
}
