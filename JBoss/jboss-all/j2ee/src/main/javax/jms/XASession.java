/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.jms;

/**
  *
  * @author Chris Kimpton (chris@kimptoc.net)
  * @version $Revision: 1.1 $
 **/
public interface XASession extends Session
{
    public javax.transaction.xa.XAResource getXAResource();

    public boolean getTransacted() throws JMSException;

    public void commit() throws JMSException;

    public void rollback() throws JMSException;


}
