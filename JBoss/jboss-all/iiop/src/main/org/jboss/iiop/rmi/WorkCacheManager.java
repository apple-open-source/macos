/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.iiop.rmi;


import java.lang.ref.SoftReference;

import java.lang.reflect.Method;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;

import java.util.Map;
import java.util.HashMap;
import java.util.WeakHashMap;


/**
 *  Instances of this class cache the most complex analyse types.
 *
 *  The analyse types cached are:
 *  <ul>
 *  <li><code>InterfaceAnalysis</code> for interfaces.</li>
 *  <li><code>ValueAnalysis</code> for value types.</li>
 *  <li><code>ExceptionAnalysis</code> for exceptions.</li>
 *  </ul>
 *
 *  Besides caching work already done, this caches work in progress,
 *  as we need to know about this to handle cyclic graphs of analyses.
 *  When a thread re-enters the <code>getAnalysis()</code> metohd, an
 *  unfinished analysis will be returned if the same thread is already
 *  working on this analysis.
 *      
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.3 $
 */
class WorkCacheManager
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   private static final org.jboss.logging.Logger logger = 
               org.jboss.logging.Logger.getLogger(WorkCacheManager.class);

   // Constructors --------------------------------------------------

   /**
    *  Create a new work cache manager.
    *
    *  @param cls The class of the analysis type we cache here.
    */
   WorkCacheManager(Class cls)
   {
      logger.debug("Class: " + cls.getName());
      // Find the constructor and initializer.
      try {
         constructor = cls.getDeclaredConstructor(new Class[]{Class.class});
         initializer = cls.getDeclaredMethod("doAnalyze", null);
      } catch (NoSuchMethodException ex) {
         throw new IllegalArgumentException("Bad Class: " + ex.toString());
      }

      workDone = new WeakHashMap();
      workInProgress = new HashMap();
   }


   // Package private -----------------------------------------------

   /**
    *  Returns an analysis.
    *  If the calling thread is currently doing an analysis of this
    *  class, an unfinished analysis is returned.
    */
   ContainerAnalysis getAnalysis(Class cls)
      throws RMIIIOPViolationException
   {
      ContainerAnalysis ret;

      synchronized (this) {
         ret = lookupDone(cls);
         if (ret != null)
            return ret;

         // is it work-in-progress?
         InProgress inProgress = (InProgress)workInProgress.get(cls);
         if (inProgress != null) {
            if (inProgress.thread == Thread.currentThread())
               return inProgress.analysis; // return unfinished 

            // Do not wait for the other thread: We may deadlock
            // Double work is better that deadlock...
         }

         ret = createWorkInProgress(cls);
      }

      // Do the work
      doTheWork(cls, ret);

      // We did it
      synchronized (this) {
         workInProgress.remove(cls);
         workDone.put(cls, new SoftReference(ret));
         notifyAll();
      }

      return ret;
   }

   // Private -------------------------------------------------------

   /**
    *  The analysis constructor of our analysis class.
    *  This constructor takes a single argument of type <code>Class</code>.
    */
   Constructor constructor;

   /**
    *  The analysis initializer of our analysis class.
    *  This method takes no arguments, and is named doAnalyze.
    */
   Method initializer;

   /**
    *  This maps the classes of completely done analyses to soft
    *  references of their analysis.
    */
   Map workDone;

   /**
    *  This maps the classes of analyses in progress to their
    *  analysis.
    */
   Map workInProgress;

   /**
    *  Lookup an analysis in the fully done map.
    */
   private ContainerAnalysis lookupDone(Class cls)
   {
      SoftReference ref = (SoftReference)workDone.get(cls);
      if (ref == null)
         return null;
      ContainerAnalysis ret = (ContainerAnalysis)ref.get();
      if (ret == null)
         workDone.remove(cls); // clear map entry if soft ref. was cleared.
      return ret;
   }

   /**
    *  Create new work-in-progress.
    */
   private ContainerAnalysis createWorkInProgress(Class cls)
   {
      ContainerAnalysis analysis;
      try {
         analysis = (ContainerAnalysis)constructor.newInstance(new Object[]{cls});
      } catch (InstantiationException ex) {
         throw new RuntimeException(ex.toString());
      } catch (IllegalAccessException ex) {
         throw new RuntimeException(ex.toString());
      } catch (InvocationTargetException ex) {
         throw new RuntimeException(ex.toString());
      }

      workInProgress.put(cls, new InProgress(analysis, Thread.currentThread()));

      return analysis;
   }

   private void doTheWork(Class cls, ContainerAnalysis ret)
      throws RMIIIOPViolationException
   {
      try {
         initializer.invoke(ret, new Object[]{});
      } catch (Throwable t) {
         synchronized (this) {
            workInProgress.remove(cls);
         }
         if (t instanceof InvocationTargetException) // unwrap
           t = ((InvocationTargetException)t).getTargetException();

         if (t instanceof RMIIIOPViolationException)
            throw (RMIIIOPViolationException)t;
         if (t instanceof RuntimeException)
            throw (RuntimeException)t;
         if (t instanceof Error)
            throw (Error)t;
         throw new RuntimeException(t.toString());
      }
   }

   /**
    *  A simple aggregate of work-in-progress, and the thread doing the work.
    */
   private static class InProgress
   {
      ContainerAnalysis analysis;
      Thread thread;

      InProgress(ContainerAnalysis analysis, Thread thread)
      {
         this.analysis = analysis;
         this.thread = thread;
      }
   }
}

