/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.resource.adapter.jms;

import javax.resource.ResourceException;
import javax.resource.spi.ConnectionManager;
import javax.resource.spi.ManagedConnectionFactory;
import javax.resource.spi.ConnectionRequestInfo;
import javax.resource.spi.ManagedConnection;

import org.jboss.logging.Logger;

/**
 * The resource adapters own ConnectionManager, used in non-managed
 * environments.
 * 
 * <p>Will handle some of the houskeeping an appserver nomaly does.
 *
 * <p>Created: Thu Mar 29 16:09:26 2001
 *
 * @author <a href="mailto:peter.antman@tim.se">Peter Antman</a>.
 * @version $Revision: 1.3 $
 */
public class JmsConnectionManager
   implements ConnectionManager
{
   private static final Logger log = Logger.getLogger(JmsConnectionManager.class);
   
   /**
    * Construct a <tt>JmsConnectionManager</tt>.
    */
   public JmsConnectionManager() {
      super();
   }

   /**
    * Allocate a new connection.
    *
    * @param mcf
    * @param cxRequestInfo
    * @return                   A new connection
    *
    * @throws ResourceException Failed to create connection.
    */
   public Object allocateConnection(ManagedConnectionFactory mcf,
                                    ConnectionRequestInfo cxRequestInfo) 
      throws ResourceException
   {
      boolean trace = log.isTraceEnabled();
      if (trace) {
         log.trace("Allocating connection; mcf=" + mcf + ", cxRequestInfo=" + cxRequestInfo);
      }
      
      ManagedConnection mc = mcf.createManagedConnection(null, cxRequestInfo);
      Object c = mc.getConnection(null, cxRequestInfo);

      if (trace) {
         log.trace("Allocated connection: " + c + ", with managed connection: " + mc);
      }
      
      return c;
   }
}
