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
public interface ObjectMessage extends Message
{
    public void setObject(java.io.Serializable object) throws JMSException;

    public java.io.Serializable getObject() throws JMSException;

}
