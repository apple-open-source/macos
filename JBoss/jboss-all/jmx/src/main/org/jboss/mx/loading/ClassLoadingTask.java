/***************************************
*                                     *
*  JBoss: The OpenSource J2EE WebOS   *
*                                     *
*  Distributable under LGPL license.  *
*  See terms of license at gnu.org.   *
*                                     *
***************************************/

package org.jboss.mx.loading;

import java.security.CodeSource;
import java.security.ProtectionDomain;
import java.util.Comparator;
import org.jboss.logging.Logger;

/** An encapsulation of a UCL3.loadClass task.
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.15 $
*/
public class ClassLoadingTask
{
   protected static Logger log = Logger.getLogger(ClassLoadingTask.class);
   protected static Comparator taskComparator = new ThreadTaskComparator();

   public static final int FOUND_CLASS_LOADER = 1;
   public static final int NEXT_EVENT = 2;
   public static final int WAIT_ON_EVENT = 3;
   public static final int FINISHED = 4;

   protected String classname;
   protected Thread requestingThread;
   protected UnifiedClassLoader requestingClassLoader;
   protected Class loadedClass;
   protected int loadOrder = Integer.MAX_VALUE;
   protected Throwable loadException;
   /** The number of ThreadTasks remaining */
   protected int threadTaskCount;
   /** The state of the requestingThread */
   protected int state;
   /** The Logger trace level flag */
   protected boolean trace;

   /** Compare ThreadTask first based on their order ivar, and then the
    * relative ordering with which their UCLs were added to the ULR.
    */
   static class ThreadTaskComparator implements Comparator
   {
      public int compare(Object o1, Object o2)
      {
         ThreadTask t1 = (ThreadTask) o1;
         ThreadTask t2 = (ThreadTask) o2;
         int compare = t1.order - t2.order;
         if( compare == 0 )
         {
            compare = t1.ucl.getAddedOrder() - t2.ucl.getAddedOrder();
         }
         return compare;
      }
   }

   /** An ecapsulation of a <Thread, UCL3> task used when requestingClassLoader
    * needs to ask another UCL3 to perform the class loading.
    */
   class ThreadTask
   {
      /** The class loader for the classname package */
      UnifiedClassLoader ucl;
      /** The thread that owns the ucl monitor */
      Thread t;
      /** The relative order of the task. If o0 < o1 then the class loaded
         by task o0 is preferred to o1.
       */
      int order;
      boolean releaseInNextTask;

      ThreadTask(UnifiedClassLoader ucl, Thread t, int order,
         boolean releaseInNextTask)
      {
         this.ucl = ucl;
         this.t = t;
         this.order = order;
         this.releaseInNextTask = releaseInNextTask;
      }

      public String toString()
      {
         return "{t="+t+", ucl="+ucl+", name="+classname
            +", requestingThread="+requestingThread
            +", order="+order+", releaseInNextTask="+releaseInNextTask
            +"}";
      }

      String getClassname()
      {
         return classname;
      }
      Class getLoadedClass()
      {
         return loadedClass;
      }
      ClassLoadingTask getLoadTask()
      {
         return ClassLoadingTask.this;
      }

      void run() throws ClassNotFoundException
      {
         Class theClass = null;
         try
         {
            if( loadedClass == null )
            {
               theClass = ucl.loadClassLocally(classname, false);
            }
            else if( trace )
            {
               log.trace("Already found class("+loadedClass+"), skipping loadClassLocally");
            }
         }
         catch(ClassNotFoundException e)
         {
            loadException = e;
            // Check to see if this is an array class
            if( classname.charAt(0) == '[' )
            {
               // First try to load the element class
               String subname = classname.substring(2, classname.length()-1);
               Class elementClass = ucl.loadClassLocally(subname, false);
               if( elementClass != null )
               {
                  // Retry loading the array class since we have the element class
                  theClass = ucl.loadClassLocally(classname, false);
                  loadException = null;
               }
            }
         }
         finally
         {
            setLoadedClass(theClass, order);
         }
      }
   }

   protected ClassLoadingTask(String classname, UnifiedClassLoader requestingClassLoader,
      Thread requestingThread)
   {
      this.requestingThread = requestingThread;
      this.requestingClassLoader = requestingClassLoader;
      this.classname = classname;
      this.trace = log.isTraceEnabled();
   }

   public String toString()
   {
      StringBuffer buffer = new StringBuffer(super.toString());
      buffer.append('{');
      buffer.append("classname: "+classname);
      buffer.append(", requestingThread: "+requestingThread);
      buffer.append(", requestingClassLoader: "+requestingClassLoader);
      buffer.append(", loadedClass: "+loadedClass);
      if( loadedClass != null )
      {
         buffer.append("@"+Integer.toHexString(loadedClass.hashCode()));
         CodeSource cs = loadedClass.getProtectionDomain().getCodeSource();
         buffer.append("<CodeSource: "+cs+">");
      }
      buffer.append(", loadOrder: "+loadOrder);
      buffer.append(", loadException: "+loadException);
      buffer.append(", threadTaskCount: "+threadTaskCount);
      buffer.append(", state: "+state);
      buffer.append('}');
      return buffer.toString();
   }

   ThreadTask newThreadTask(UnifiedClassLoader ucl, Thread t, int order,
      boolean reschedule, boolean releaseInNextTask)
   {
      // Only update the threadTaskCount if this is not a reschedule
      if( reschedule == false )
         threadTaskCount ++;
      return new ThreadTask(ucl, t, order, releaseInNextTask);
   }

   /** This is called from run on success or failure to mark the end
    * of the load attempt. This must decrement the threadTaskCount or
    * the ClassLoadingTask will never complete.
    */
   private synchronized void setLoadedClass(Class theClass, int order)
   {
      this.threadTaskCount --;
      if( trace )
         log.trace("setLoadedClass, theClass="+theClass+", order="+order);

      // Warn about duplicate classes
      if( this.loadedClass != null && order == loadOrder && theClass != null )
      {
         StringBuffer tmp = new StringBuffer("Duplicate class found: "+classname);
         tmp.append('\n');
         ProtectionDomain pd = this.loadedClass.getProtectionDomain();
         CodeSource cs = pd != null ? pd.getCodeSource() : null;
         tmp.append("Current CS: "+cs);
         tmp.append('\n');
         pd = theClass.getProtectionDomain();
         cs = pd != null ? pd.getCodeSource() : null;
         tmp.append("Duplicate CS: "+cs);
         log.warn(tmp.toString());
      }

      // Accept the lowest order source of the class
      if( theClass != null )
      {
         if( loadedClass == null || order <= loadOrder )
         {
            this.loadedClass = theClass;
            this.loadOrder = order;
         }
         else
         {
            ProtectionDomain pd = this.loadedClass.getProtectionDomain();
            CodeSource cs = pd != null ? pd.getCodeSource() : null;
            ProtectionDomain pd2 = theClass.getProtectionDomain();
            CodeSource cs2 = pd != null ? pd2.getCodeSource() : null;
            log.debug("Ignoring source of: "+classname+" from CodeSource: "+cs2
               +", due to order("+order+">="+loadOrder+"), "
               +"accepted CodeSource: "+cs);
         }
      }
   }
}
