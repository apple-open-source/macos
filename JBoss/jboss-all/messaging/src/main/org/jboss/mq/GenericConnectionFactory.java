/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;

import java.util.Properties;
import javax.jms.JMSException;
import org.jboss.mq.il.ClientIL;
import org.jboss.mq.il.ClientILService;

import org.jboss.mq.il.ServerIL;
import org.jboss.mq.il.ServerILFactory;

import org.jboss.logging.Logger;

/**
 *  The RMI implementation of the DistributedConnectionFactory object
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.6 $
 */
public class GenericConnectionFactory implements java.io.Serializable {

   static Logger log = Logger.getLogger( GenericConnectionFactory.class );

   // An instance of the ServerIL, once it is setup, we make clones every
   // time a new connection is needed.
   private ServerIL server;

   // Holds all the information need to connect to the server.
   // And to setup a connection from the server to the client.
   private Properties connectionProperties;


   /**
    *  The constructor takes a ServerIL and the Connection Properties
    *  parameters, The connection properties are allways required since they are
    *  used to setup the ClientIL, but the ServerIL can be null if the
    *  connection properties defines a ServerILFactory so that the SeverIL can
    *  be created on the client side. The ServerIL paramter is usefull for IL
    *  such as RMI or the JVM IL since trying to explicity create a connection
    *  to them is not strait forward.
    *
    * @param  server  Description of Parameter
    * @param  props   Description of Parameter
    */
   public GenericConnectionFactory( ServerIL server, Properties props ) {
      this.server = server;
      this.connectionProperties = props;
   }

   /**
    *  Creates a new instance of the ClientILService
    *
    * @param  connection     Description of Parameter
    * @return                Description of the Returned Value
    * @exception  Exception  Description of Exception
    */
   public ClientILService createClientILService( Connection connection )
      throws Exception {
      // This is a good time to setup the PingPeriod
      String pingPeriod = connectionProperties.getProperty( ServerILFactory.PING_PERIOD_KEY, ""+connection.pingPeriod ); 
      connection.pingPeriod = Long.parseLong(pingPeriod);
      
      // Setup the client connection.
      String clientILServiceCN = connectionProperties.getProperty( ServerILFactory.CLIENT_IL_SERVICE_KEY );
      ClientILService service = ( ClientILService )Class.forName( clientILServiceCN ).newInstance();
      service.init( connection, connectionProperties );

      if ( log.isDebugEnabled() )
         log.debug("Handing out ClientIL: " + clientILServiceCN);

      return service;
   }

   /**
    *  Creates a new instance of the ServerIL
    *
    * @return                   Description of the Returned Value
    * @exception  JMSException  Description of Exception
    */
   public ServerIL createServerIL()
      throws JMSException {

      try {

         // The server was not set, so lets try to set it up with
         // A ServerILFactory
         if ( server == null ) {

            String className = connectionProperties.getProperty( ServerILFactory.SERVER_IL_FACTORY_KEY );
            ServerILFactory factory = ( ServerILFactory )Class.forName( className ).newInstance();
            factory.init( connectionProperties );

            server = factory.getServerIL();
         }

         // We clone because one ConnectionFactory instance can be
         // used to produce multiple connections.
         return server.cloneServerIL();
      } catch ( Exception e ) {
         log.error( "Could not connect to the server", e );
         throw new SpyJMSException( "Could not connect to the server", e );
      }
   }
   
   public String toString() {
   	return "GenericConnectionFactory:[server="+server+",connectionProperties="+connectionProperties+"]";
   }
}
