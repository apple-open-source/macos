/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.threadpool;

/**
 *  This is the interface of work that the thread pool can do.
 *
 *  Users of the thread pool enqueue an object implementing this
 *  interface to have one of the threads in the thread pool call
 *  back the method declared here.
 *
 * @author    Ole Husgaard (osh@sparre.dk)
 * @version   $Revision: 1.1 $
 */
public interface Work
{
   /**
    *  Callback to do the actual work.
    */
   public void doWork();
}
