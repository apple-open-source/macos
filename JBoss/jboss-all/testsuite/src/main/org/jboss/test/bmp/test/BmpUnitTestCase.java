/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.bmp.test;


import javax.naming.*;
import java.util.Collection;
import java.util.Iterator;

import org.jboss.test.bmp.interfaces.*;

import junit.framework.*;

import org.jboss.test.JBossTestCase;

public class BmpUnitTestCase
    extends JBossTestCase
{
    private boolean deployed;

    public BmpUnitTestCase(String name)
    {
        super(name);
    }

    public void testBMP() throws Exception
   {
      BMPHelperSessionHome sessionHome = (BMPHelperSessionHome)new InitialContext ().lookup ("bmp.BMPHelperSession");
      BMPHelperSession session = sessionHome.create ();
      
      getLog().debug ("looking up table:");
      boolean exists =  session.existsSimpleBeanTable ();
      
      if (exists)
      {
         getLog().debug ("table exists.");
         getLog().debug ("delete it...");
         session.dropSimpleBeanTable();
         getLog().debug ("done.");
      }
      else
      {
         getLog().debug ("table does not exist.");
         getLog().debug ("create it...");
         session.createSimpleBeanTable();
         getLog().debug ("done.");
         
         getLog().debug ("start playing with bmp beans.");
         SimpleBMPHome home = (SimpleBMPHome)new InitialContext ().lookup ("bmp.SimpleBMP");
         getLog().debug ("create bean1: 1, Daniel");
         SimpleBMP b1 = home.create (1, "Daniel");
         getLog().debug ("getName (): "+b1.getName ());

         getLog().debug ("create bean2: 2, Robert");
         b1 = home.createMETHOD (2, "Robert");
         getLog().debug ("getName (): "+b1.getName ());

         try
         {
            getLog().debug ("trying to create one with same primkey: 1, Patrick");
            b1 = home.create (1, "Patrick");
         }
         catch (Exception _e)
         {
            getLog().debug (_e.toString ());
         }
         
         getLog().debug ("create some more dummys:");
         for (int i = 0; i < 50; ++i)
            home.create (i + 3, ("Dummy "+i));

         try
         {
            getLog().debug ("trying to find Robert again");
            b1 = home.findByPrimaryKey (new Integer (2));
            getLog().debug ("getName (): "+b1.getName ());
         }
         catch (Exception _e)
         {
            getLog().debug (_e.toString ());
         }

         try
         {
            getLog().debug ("trying to find an not existing bean");
            b1 = home.findByPrimaryKey (new Integer (0));
            getLog().debug ("getName (): "+b1.getName ());
         }
         catch (Exception _e)
         {
            getLog().debug (_e.toString ());
         }
         
         
         getLog().debug ("rename Daniel to Maria: 1, Daniel");
         b1 = home.findByPrimaryKey (new Integer (1));
         getLog().debug ("name old: " + b1.getName ());
         b1.setName ("Maria");
         getLog().debug ("name new: " + b1.getName ());
        
         
         getLog().debug ("find all beans:");
         Iterator it = home.findAll ().iterator ();
         while (it.hasNext ())
         {
            getLog().debug ("found:"+((SimpleBMP)it.next ()).getName ());
         }            


         getLog().debug ("*******Now trying from within the Session bean (to be able to rollback):");
         getLog().debug (session.doTest ());
         getLog().debug ("Getting the name after a rollback:");
         getLog().debug (session.doTestAfterRollback ());

         getLog().debug ("removing all beans:");
         it = home.findAll ().iterator ();
         while (it.hasNext ())
            ((SimpleBMP)it.next ()).remove ();
      }

   }

   public static Test suite() throws Exception
   {
      return getDeploySetup(BmpUnitTestCase.class, "bmp.jar");
   }


}
