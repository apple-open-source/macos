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
public interface ConnectionMetaData
{
    public int                      getJMSMajorVersion() throws JMSException;
    public int                      getJMSMinorVersion() throws JMSException;
    public String                   getJMSProviderName() throws JMSException;
    public String                   getJMSVersion() throws JMSException;
    public java.util.Enumeration    getJMSXPropertyNames() throws JMSException;
    public int                      getProviderMajorVersion() throws JMSException;
    public int                      getProviderMinorVersion() throws JMSException;
    public String                   getProviderVersion() throws JMSException;

}
