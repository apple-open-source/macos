package org.jboss.mq.il.uil2;

import org.jboss.mq.il.uil2.msgs.BaseMsg;
import org.jboss.util.stream.StreamListener;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.2 $
 */
public interface SocketManagerHandler
   extends StreamListener
{
   /**
    * Handle the message
    * @param msg the message to handler
    * @exception Exception for any error
    */
   public void handleMsg(BaseMsg msg) throws Exception;

   /**
    * Handle a stream notification
    *
    * @param stream the stream
    * @param size the bytes since the last notification
    */
   public void onStreamNotification(Object stream, int size);

   /**
    * Report a connection failure
    * @param error the error text
    * @param throwable the error
    */
   public void asynchFailure(String error, Throwable e);

   /**
    * Handle closedown, this maybe invoked many times
    * due to an explicit close and/or a connection failure.
    */
   public void close();
}
