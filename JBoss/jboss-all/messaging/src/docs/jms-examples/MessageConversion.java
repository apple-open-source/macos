/*
 * @(#)MessageConversion.java	1.4 00/08/09
 * 
 * Copyright (c) 2000 Sun Microsystems, Inc. All Rights Reserved.
 * 
 * Sun grants you ("Licensee") a non-exclusive, royalty free, license to use,
 * modify and redistribute this software in source and binary code form,
 * provided that i) this copyright notice and license appear on all copies of
 * the software; and ii) Licensee does not utilize the software in a manner
 * which is disparaging to Sun.
 *
 * This software is provided "AS IS," without a warranty of any kind. ALL
 * EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
 * NON-INFRINGEMENT, ARE HEREBY EXCLUDED. SUN AND ITS LICENSORS SHALL NOT BE
 * LIABLE FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING
 * OR DISTRIBUTING THE SOFTWARE OR ITS DERIVATIVES. IN NO EVENT WILL SUN OR ITS
 * LICENSORS BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA, OR FOR DIRECT,
 * INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR PUNITIVE DAMAGES, HOWEVER
 * CAUSED AND REGARDLESS OF THE THEORY OF LIABILITY, ARISING OUT OF THE USE OF
 * OR INABILITY TO USE SOFTWARE, EVEN IF SUN HAS BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * This software is not designed or intended for use in on-line control of
 * aircraft, air traffic, aircraft navigation or aircraft communications; or in
 * the design, construction, operation or maintenance of any nuclear
 * facility. Licensee represents and warrants that it will not use or
 * redistribute the Software for such purposes.
 */
import javax.jms.*;

/**
 * The MessageConversion class consists only of a main method, which creates 
 * and then reads a StreamMessage and a BytesMessage.  It does not send the 
 * messages.
 * <p>
 * The program demonstrates type conversions in StreamMessages:  you can write
 * data as a String and read it as an Int, and vice versa.  The program also
 * calls clearBody() to clear the message so that it can be rewritten.
 * <p>
 * The program also shows how to write and read a BytesMessage using data types
 * other than a byte array.  Conversion between String and other types is
 * not supported.
 * <p>
 * Before it can read a BytesMessage or StreamMessage that has not been sent,
 * the program must call reset() to put the message body in read-only mode 
 * and reposition the stream.
 *
 * @author Kim Haase
 * @version 1.4, 08/09/00
 */
public class MessageConversion {

    /**
     * Main method.  Takes no arguments.
     */
    public static void main(String[] args) {
        QueueConnectionFactory  queueConnectionFactory = null;
        QueueConnection         queueConnection = null;
        QueueSession            queueSession = null;
        BytesMessage            bytesMessage = null;
        StreamMessage           streamMessage = null;
        int                     exitResult = 0;
        
        try {
            queueConnectionFactory = 
                SampleUtilities.getQueueConnectionFactory();
            queueConnection = 
                queueConnectionFactory.createQueueConnection();
            queueSession = queueConnection.createQueueSession(false, 
                Session.AUTO_ACKNOWLEDGE);
    	} catch (Exception e) {
            System.out.println("Connection problem: " + e.toString());
            if (queueConnection != null) {
                try {
                    queueConnection.close();
                } catch (JMSException ee) {}
            }
    	    System.exit(1);
    	} 

        try {
            /* 
             * Create a StreamMessage and write values of various data types
             * to it.
             * Reset the message, then read the values as Strings.
             * Values written to a StreamMessage as one data type can be read 
             * as Strings and vice versa (except for String to char conversion).
             */
            streamMessage = queueSession.createStreamMessage();
    	    streamMessage.writeBoolean(false);
    	    streamMessage.writeDouble(123.456789e222);
    	    streamMessage.writeInt(223344);
    	    streamMessage.writeChar('q');
            streamMessage.reset();
            System.out.println("Reading StreamMessage items of various data"
                + " types as String:");
            System.out.println(" Boolean: " + streamMessage.readString());
            System.out.println(" Double: " + streamMessage.readString());
            System.out.println(" Int: " + streamMessage.readString());
            System.out.println(" Char: " + streamMessage.readString());
            
            /*
             * Clear the body of the StreamMessage and write several Strings
             * to it.
             * Reset the message and read the values back as other data types.
             */
            streamMessage.clearBody();
            streamMessage.writeString("true");
            streamMessage.writeString("123.456789e111");
            streamMessage.writeString("556677");
            // Not char:  String to char conversion isn't valid
            streamMessage.reset();
            System.out.println("Reading StreamMessage String items as other"
                + " data types:");
            System.out.println(" Boolean: " + streamMessage.readBoolean());
            System.out.println(" Double: " + streamMessage.readDouble());
            System.out.println(" Int: " + streamMessage.readInt());
            
            /* 
             * Create a BytesMessage and write values of various types into
             * it.
             */
            bytesMessage = queueSession.createBytesMessage();
    	    bytesMessage.writeBoolean(false);
    	    bytesMessage.writeDouble(123.456789e22);
    	    bytesMessage.writeInt(778899);
    	    bytesMessage.writeInt(0x7f800000);
    	    bytesMessage.writeChar('z');
    	    
    	    /*
    	     * Reset the message and read the values back.  Only limited
    	     * type conversions are possible.
    	     */
            bytesMessage.reset();
            System.out.println("Reading BytesMessages of various types:");
            System.out.println(" Boolean: " + bytesMessage.readBoolean());
            System.out.println(" Double: " + bytesMessage.readDouble());
            System.out.println(" Int: " + bytesMessage.readInt());
            System.out.println(" Float: " + bytesMessage.readFloat());
            System.out.println(" Char: " + bytesMessage.readChar());
        } catch (JMSException e) {
            System.out.println("JMS Exception occurred: " + e.toString());
            exitResult = 1;
        } finally {
            if (queueConnection != null) {
                try {
                    queueConnection.close();
                } catch (JMSException e) {
                    exitResult = 1;
                }
            }
        }
        SampleUtilities.exit(exitResult);
    }
}

