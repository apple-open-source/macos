package org.jboss.test.load.test;

import java.util.Hashtable;

import javax.naming.Context;
import javax.naming.InitialContext;
import java.lang.reflect.UndeclaredThrowableException;

import javax.ejb.*;

import org.jboss.test.testbean.interfaces.*;

/**
* Working thread for the Client class. <br>
* looks up the number of beans he shall use (creates them if not found)
* and reads a value and writes a value for every iteration on every bean.
* After completition the beans are removed on fail the beans are not removed.
*
* @author <a href="mailto:daniel.schulze@telkel.com">Daniel Schulze</a>
* @version $Id: Worker.java,v 1.4 2002/02/15 06:15:54 user57 Exp $
*/
public class Worker
extends Thread
{
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
   
   // succcess or not?
   Hashtable config;

   Worker (ThreadGroup _group, Hashtable _config)
   {
      super (_group, (String)_config.get ("name") + "_" + _config.get("number").toString ());
      config = _config;
   }


   /** for the client to ask for success */
   public Hashtable getConfig ()
   {
      return config;
   }

   public void run()
   {
      // get the configuration
      int beans = ((Integer)config.get ("beans")).intValue ();
      int loops = ((Integer)config.get ("loops")).intValue ();
      boolean verbose = ((Boolean)config.get ("verbose")).booleanValue ();
      boolean remove = !((Boolean)config.get ("noremove")).booleanValue ();
      String logTag = "["+getName ()+"] ";

      int txs = 0;
      long start =  System.currentTimeMillis ();

      EnterpriseEntity[] entity = new EnterpriseEntity[beans];
      try
      {
         // find/create the beans
         EnterpriseEntityHome home = (EnterpriseEntityHome) new InitialContext ().lookup ("nextgen.EnterpriseEntity");
         if (home == null)
         {
            log.debug (logTag + "EJBHomeInterface lookup returned null?!");
            log.debug (logTag + "died.");
            prepareExit (true, txs, start, System.currentTimeMillis ());
            return;
         }
         for (int i = 0; i < beans; ++i)
         {
            String key = getName () + "_" + i;
            try
            {
               // first try to find it...
               entity[i] = home.findByPrimaryKey (key);
               if (entity[i] == null)
               {
                  log.debug (logTag + "EJBHome.findByPrimaryKey () returned null?!");
                  log.debug (logTag + "died.");
                  prepareExit (true, txs, start, System.currentTimeMillis ());
                  return;
               }
               ++txs;
               if (verbose) log.debug (logTag + "reuse bean: "+entity[i].getPrimaryKey ());
            }
            catch (FinderException _fe)
            {
               // so lets create it...
               entity[i] = home.create (key);
               if (entity[i] == null)
               {
                  log.debug (logTag + "EJBHome.create () returned null?!");
                  log.debug (logTag + "died.");
                  prepareExit (true, txs, start, System.currentTimeMillis ());
                  return;
               }
               ++txs;
               if (verbose) log.debug (logTag + "create bean: "+entity[i].getPrimaryKey ());
            }
         }
      }
      catch (Exception _e)
      {
         log.debug (logTag+ "problem while finding/creating beans: "+_e.toString ());
         if (_e instanceof UndeclaredThrowableException) 
            log.debug(logTag+ "target exception: " + ((UndeclaredThrowableException)_e).getUndeclaredThrowable().toString());
         
         log.debug (logTag+" died!");
         prepareExit (true, txs, start, System.currentTimeMillis ());
         return;
      }

      // prepare the beans for the test.....
      for (int b = 0; b < beans; ++b)
      {
         try
         {
            entity[b].setOtherField (0);
            ++txs;
         }
         catch (Exception _e)
         {
            log.debug (logTag+ "cannot prepare bean #"+b+": "+_e.toString ());
            log.debug (logTag+" died!");
            prepareExit (true, txs, start, System.currentTimeMillis ());
            return;
         }
      }

      // do the test...
      for (int l = 0; l < loops; ++l)
      {
         for (int b = 0; b < beans; ++b)
         {
            int value = -1;
            try
            {
               value = entity[b].getOtherField ();
               ++txs;
            }
            catch (Exception _e)
            {
               prepareExit (true, txs, start, System.currentTimeMillis ());
               log.debug (logTag + "error in getOtherField () on bean #"+b+" in loop "+l+ ": "+_e.toString ());
               if (value != l)
                  log.debug (logTag + "unexpected value in bean #"+b+" in loop "+l);
            }

            try
            {
               entity[b].setOtherField (l + 1);
               ++txs;
            }
            catch (Exception _e)
            {
               prepareExit (true, txs, start, System.currentTimeMillis ());
               log.debug (logTag + "error in setOtherField () on bean #"+b+" in loop "+l+ ": "+_e.toString ());
            }
         }
      }


      // remove the beans...
      if (remove) 
      {
         for (int b = 0; b < beans; ++b)
         {
            try
            {
               if (verbose) log.debug (logTag + "remove bean #"+b);
               entity[b].remove ();
               ++txs;
            }
            catch (Exception _e)
            {
               log.debug (logTag + "error in removing bean #"+b+": "+_e.toString ());
               prepareExit (true, txs, start, System.currentTimeMillis ());
               //_e.printStackTrace (System.out);
            }
         }
      }
      prepareExit (false, txs, start, System.currentTimeMillis ());
   }
   
   private void prepareExit (boolean _failed, int _transactions, long _start, long _stop)
   {
      config.put ("transactions", new Integer (_transactions));
      config.put ("time", new Long (_stop-_start));
      config.put ("failed", new Boolean (_failed));
   }
   
}
