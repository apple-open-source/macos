/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.jms;

/**
  * A JMS Connection is a client's active connection to its JMS provider.
  * It will typically allocate provider resources outside the Java virtual machine.
  * <p>
  * Connections support concurrent use.
  * <p>
  * A Connection serves several purposes:
  * <p>
  * It encapsulates an open connection with a JMS provider. It typically represents
  * an open TCP/IP socket between a client and a provider service daemon.
  * Its creation is where client authenticating takes place.
  * It can specify a unique client identifier.
  * It provides ConnectionMetaData.
  * It supports an optional ExceptionListener.
  * Due to the authentication and communication setup done when a Connection is
  * created, a Connection is a relatively heavy-weight JMS object. Most clients
  * will do all their messaging with a single Connection. Other more advanced
  * applications may use several Connections. JMS does not architect a reason
  * for using multiple connections; however, there may be operational reasons for doing so.
  *  <p>
  * A JMS client typically creates a Connection; one or more Sessions; and a
  * number of message producers and consumers. When a Connection is created it
  * is in stopped mode. That means that no messages are being delivered.
  *  <p>
  * It is typical to leave the Connection in stopped mode until setup is complete.
  * At that point the Connection's start() method is called and messages begin
  * arriving at the Connection's consumers. This setup convention minimizes any
  * client confusion that may result from asynchronous message delivery while the
  * client is still in the process of setting itself up.
  *  <p>
  * A Connection can immediately be started and the setup can be done afterwards.
  * Clients that do this must be prepared to handle asynchronous message delivery
  * while they are still in the process of setting up.
  *  <p>
  * A message producer can send messages while a Connection is stopped
  *
  *
  * @author Chris Kimpton (chris@kimptoc.net)
  * @version $Revision: 1.1 $
 **/
public interface Connection
{


   /**
     * Get the client identifier for this connection. This value is JMS Provider
     * specific. Either pre-configured by an administrator in a ConnectionFactory
     * or assigned dynamically by the application by calling setClientID method.
     *
     * @return the client identifier for this connection
     * @throws JMSException if JMS implementation fails to return the client ID for this Connection due to some internal error.
     */
   public String getClientID() throws JMSException;



   /**
     * Set the client identifier for this connection.
     * The preferred way to assign a Client's client identifier is for it to be configured in a client-specific ConnectionFactory and transparently assigned to the Connection it creates.
     *  <p>
     * Alternatively, a client can set a connection's client identifier using a provider-specific value. The facility to explicitly set a connection's client identifier is not a mechanism for overriding the identifier that has been administratively configured. It is provided for the case where no administratively specified identifier exists. If one does exist, an attempt to change it by setting it must throw a IllegalStateException. If a client explicitly does the set it must do this immediately after creating the connection and before any other action on the connection is taken. After this point, setting the client identifier is a programming error that should throw an IllegalStateException.
     *  <p>
     * The purpose of client identifier is to associate a connection and its objects with a state maintained on behalf of the client by a provider. The only such state identified by JMS is that required to support durable subscriptions
     *  <p>
     * If another connection with clientID is already running when this method is called, the JMS Provider should detect the duplicate id and throw InvalidClientIDException.
     *
     * @param theClientID   the client identifier for this connection
     * @throws JMSException general exception if JMS implementation fails to set the client ID for this Connection due to some internal error.
     * @throws InvalidClientIDException if JMS client specifies an invalid or duplicate client id.
     * @throws IllegalStateException if attempting to set a connection's client identifier at the wrong time or when it has been administratively configured.
     */
   public void setClientID(String theClientID) throws JMSException;

   /**
     * Get the meta data for this connection
     *
     * @return the meta data for this connection
     * @throws JMSException general exception if JMS implementation fails to get the Connection meta-data for this Connection
     */
   public ConnectionMetaData getMetaData() throws JMSException;


   /**
     * Get the ExceptionListener for this Connection
     *
     * @return the ExceptionListener for this Connection.
     * @throws JMSException general exception if JMS implementation fails to get the Exception listener for this Connection.
     */
   public ExceptionListener getExceptionListener() throws JMSException;

   public void setExceptionListener(ExceptionListener theListener) throws JMSException;

   public void start() throws JMSException;

   public void stop() throws JMSException;

   public void close() throws JMSException;
}
