package org.jboss.verifier;

/*
 * Class org.jboss.verifier.Main;
 * Copyright (C) 2000  Juha Lindfors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This package and its source code is available at www.jboss.org
 * $Id: Main.java,v 1.6 2002/05/31 13:02:14 lqd Exp $
 *
 * You can reach the author by sending email to jplindfo@helsinki.fi.
 */

// standard imports
import java.io.File;
import java.net.URL;
import java.net.URLClassLoader;

// non-standard class dependencies
import org.jboss.verifier.event.VerificationListener;
import org.jboss.verifier.event.VerificationEvent;

import org.jboss.metadata.XmlFileLoader;

/**
 * Main class for bean verifier.
 *
 * For more detailed documentation, refer to the
 * <a href="http://www.ejboss.org">JBoss project</a>
 *
 * @author 	Juha Lindfors
 * @version $Revision: 1.6 $
 * @since  	JDK 1.3
 */
public class Main
{
   public final static int OK      = 0;
   public final static int WARNING = 1;

   static int returnCode = OK;

   /**
    * Starts the application.
    *
    * @param   args    argument strings
    */
   public static void main(String[] args)
   {
      try
      {
         if( args.length < 1 )
         {
            throw new IllegalArgumentException(
               "Usage: beanverifier mybeans.jar");
         }

         URL url = new File(args[0]).toURL();
         ClassLoader cl = new URLClassLoader( new URL[] {url},
            Thread.currentThread().getContextClassLoader());
         XmlFileLoader xfl = new XmlFileLoader();
         BeanVerifier verifier = new BeanVerifier();

         xfl.setClassLoader(cl);
         verifier.addVerificationListener(new Listener());

         verifier.verify(url, xfl.load());
      }
      catch (Exception e)
      {
         System.err.println("Problem starting the application:");
         System.err.println("Exception: " + e);
         System.err.println("Message:   " + e.getMessage());
         e.printStackTrace();

         System.exit(-1);
      }

      System.exit(returnCode);
   }
}

class Listener
   implements VerificationListener
{
   public void specViolation(VerificationEvent event)
   {
      System.out.println(event.getVerbose());

      Main.returnCode = Main.WARNING;
   }

   public void beanChecked(VerificationEvent event)
   {
      System.out.println(event.getMessage());
   }
}
/*
vim:ts=3:sw=3:et
*/
