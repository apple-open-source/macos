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
public interface MapMessage extends Message
{
    public boolean getBoolean(String name) throws JMSException;
    public byte getByte(String name) throws JMSException;
    public short getShort(String name) throws JMSException;
    public char getChar(String name) throws JMSException;
    public int getInt(String name) throws JMSException;
    public long getLong(String name) throws JMSException;
    public float getFloat(String name) throws JMSException;
    public double getDouble(String name) throws JMSException;
    public String getString(String name) throws JMSException;

    public byte[] getBytes(String name) throws JMSException;

    public Object getObject(String name) throws JMSException;

    public java.util.Enumeration getMapNames() throws JMSException;

    public void setBoolean(String name, boolean value) throws JMSException;
    public void setByte(String name, byte value) throws JMSException;
    public void setShort(String name, short value) throws JMSException;
    public void setChar(String name, char value) throws JMSException;
    public void setInt(String name, int value) throws JMSException;
    public void setLong(String name, long value) throws JMSException;
    public void setFloat(String name, float value) throws JMSException;
    public void setDouble(String name, double value) throws JMSException;
    public void setString(String name, String value) throws JMSException;

    public void setBytes(String name, 
                         byte[] value) throws JMSException;
    public void setBytes(String name, 
                         byte[] value,
                         int offset,
                         int length) throws JMSException;

    public void setObject(String name, Object value) throws JMSException;

    public boolean itemExists(String name) throws JMSException;

}

