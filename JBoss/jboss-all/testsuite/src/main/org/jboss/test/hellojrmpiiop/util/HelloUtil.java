/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.hellojrmpiiop.util;

import java.io.*;
import java.security.*;

/**
 *      
 *   @see <related>
 *   @author $Author: reverbel $
 *   @version $Revision: 1.1 $
 */
public class HelloUtil
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
   
   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   
   // Public --------------------------------------------------------
   public InputStream getResource(final String name, final Object req)
   {
      return (InputStream)AccessController.doPrivileged(new PrivilegedAction()
      {
         public Object run()
         {
            return req.getClass().getClassLoader().getResourceAsStream(name);
         }
      });
   }
}
