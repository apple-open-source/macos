/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

import java.io.Serializable;

/**
 * Support for evaluting a query in the context of an MBeanServer.
 *
 * <p><b>Revisions:</b>
 * <p><b>20020317 Adrian Brock:</b>
 * <ul>
 * <li>Make queries thread safe
 * </ul>
 * 
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.2 $
 */
public abstract class QueryEval
   implements Serializable
{
   // Constants ---------------------------------------------------

   // Attributes --------------------------------------------------

   // Static ------------------------------------------------------

   /**
    * The MBeanServer (one per thread)
    */
   /*package*/ static ThreadLocal server = new ThreadLocal();

   // Public ------------------------------------------------------

   /**
    * Set the MBeanServer for this query. Only MBeans registered in
    * this server can be used in queries.
    *
    * @param server the MBeanServer
    */
   public void setMBeanServer(MBeanServer server)
   {
      this.server.set(server);
   }
}
