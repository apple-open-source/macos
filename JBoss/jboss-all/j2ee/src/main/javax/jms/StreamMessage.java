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
public interface StreamMessage extends Message
{
    public boolean readBoolean() throws JMSException;
    public byte readByte() throws JMSException;
    public short readShort() throws JMSException;
    public char readChar() throws JMSException;
    public int readInt() throws JMSException;
    public long readLong() throws JMSException;
    public float readFloat() throws JMSException;
    public double readDouble() throws JMSException;
    public String readString() throws JMSException;

    public int readBytes(byte[] value) throws JMSException;
    public Object readObject() throws JMSException;


    public void writeBoolean(boolean value) throws JMSException;
    public void writeByte(byte value) throws JMSException;
    public void writeShort(short value) throws JMSException;
    public void writeChar(char value) throws JMSException;
    public void writeInt(int value) throws JMSException;
    public void writeLong(long value) throws JMSException;
    public void writeFloat(float value) throws JMSException;
    public void writeDouble(double value) throws JMSException;
    public void writeString(String value) throws JMSException;

    public void writeBytes(byte[] value) throws JMSException;
    public void writeBytes(byte[] value,
                           int offset,
                           int length) throws JMSException;

    public void writeObject(Object value) throws JMSException;

    public void reset() throws JMSException;

    
}
