/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.invocation.pooled.server;


import org.jboss.util.LRUCachePolicy;

/**
 * This class is an extention of LRUCachePolicy.  On a entry removal
 * it makes sure to call shutdown on the pooled ServerThread
 *
 * @author    <a href="mailto:bill@jboss.org">Bill Burke</a>
 *
 */
public class LRUPool extends LRUCachePolicy
{
   public LRUPool(int min, int max)
   {
      super(min, max);
   }
   protected void entryRemoved(LRUCachePolicy.LRUCacheEntry entry) 
   {
      ServerThread thread = (ServerThread)entry.m_object;
      thread.evict();
   }

   public void evict()
   {
      // the entry will be removed by ageOut
      ServerThread thread = (ServerThread)m_list.m_tail.m_object;
      thread.evict();
   }
   
}
