
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.resource.connectionmanager;


import java.util.LinkedList;


/**
 * PoolFiller.java
 *
 *
 * Created: Fri Jan  4 13:35:21 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class PoolFiller 
{

   private final LinkedList pools = new LinkedList();

   private final Thread fillerThread;

   private static final PoolFiller filler = new PoolFiller();

   public static void fillPool(InternalManagedConnectionPool mcp)
   {
      filler.internalFillPool(mcp);
   }

   public PoolFiller ()
   {
      fillerThread = new Thread(
         new Runnable() {
            public void run()
            {
               while (true)//keep going unless interrupted
               {
                  try 
                  {
                     InternalManagedConnectionPool mcp = null;
                     while (true)//keep iterating through pools till empty, exception escapes.
                     {
                     
                        synchronized (pools)
                        {
                           mcp = (InternalManagedConnectionPool)pools.removeFirst();
                        }
                        if (mcp == null) 
                        {
                           break;
                        } // end of if ()
                        
                        mcp.fillToMin();
                     } // end of while (true)

                  }
                  catch (Exception e)
                  {//end of pools list
                  } // end of try-catch
                        
                  try 
                  {
                     synchronized (pools)
                     {
                        pools.wait();                        
                     }
                  }
                  catch (InterruptedException ie)
                  {
                     return;
                  } // end of try-catch
               } // end of while ()
            }  
         });
      fillerThread.start();
   }

   private void internalFillPool(InternalManagedConnectionPool mcp)
   {
      synchronized (pools)
      {
         pools.addLast(mcp);
         pools.notify();
      }
   }
   
}// PoolFiller
