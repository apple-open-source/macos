/*
 * @(#)TextListener.java	1.5 00/08/09
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
 * The TextListener class implements the MessageListener interface by 
 * defining an onMessage method that displays the contents of a TextMessage.
 * <p>
 * This class acts as the listener for the AsynchQueueReceiver class.
 *
 * @author Kim Haase
 * @version 1.5, 08/09/00
 */
public class TextListener implements MessageListener {
    final SampleUtilities.DoneLatch  monitor = 
        new SampleUtilities.DoneLatch();

    /**
     * Casts the message to a TextMessage and displays its text.
     * A non-text message is interpreted as the end of the message 
     * stream, and the message listener sets its monitor state to all 
     * done processing messages.
     *
     * @param message	the incoming message
     */
    public void onMessage(Message message) {
        if (message instanceof TextMessage) {
            TextMessage  msg = (TextMessage) message;
            
            try {
                System.out.println("Reading message: " + msg.getText());
            } catch (JMSException e) {
                System.out.println("Exception in onMessage(): " + e.toString());
            }
        } else {
            monitor.allDone();
        }
    }
}

