/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;

import java.io.Externalizable;
import java.io.ObjectOutput;
import java.io.ObjectInput;
import java.io.IOException;

import java.util.ArrayList;

import javax.jms.JMSException;
import javax.jms.MessageNotWriteableException;
import javax.jms.TextMessage;

/**
 * This class implements javax.jms.TextMessage
 *
 * @author Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @created August 16, 2001
 * @version $Revision: 1.10 $
 */
public class SpyTextMessage
   extends SpyMessage
   implements Cloneable, TextMessage, Externalizable
{
   // Attributes ----------------------------------------------------

   String content;

   private final static long serialVersionUID = 235726945332013953L;
   
   private final static int chunkSize = 16384;

   // Public --------------------------------------------------------

   public void setText(String string)
      throws JMSException
   {
      if (header.msgReadOnly)
      {
         throw new MessageNotWriteableException("Cannot set the content; message is read-only");
      }
      
      content = string;
   }

   public String getText()
      throws JMSException
   {
      return content;
   }

   public void clearBody()
      throws JMSException
   {
      content = null;
      super.clearBody();
   }

   public SpyMessage myClone()
      throws JMSException
   {
      SpyTextMessage result = MessagePool.getTextMessage();
      result.copyProps(this);
      result.content = this.content;
      return result;
   }

   public void readExternal(ObjectInput in)
      throws IOException, ClassNotFoundException
   {
      super.readExternal(in);
      byte type = in.readByte();
      
      if (type == NULL)
      {
         content = null;
      }
      else
      {
         // apply workaround for string > 64K bug in jdk's 1.3.*

         // Read the no. of chunks this message is split into, allocate
         // a StringBuffer that can hold all chunks, read the chunks
         // into the buffer and set 'content' accordingly
         int chunksToRead = in.readInt();
         int bufferSize = chunkSize * chunksToRead;

         // special handling for single chunk
	 if (chunksToRead == 1)
         {
            // The text size is likely to be much smaller than the chunkSize
            // so set bufferSize to the min of the input stream available
            // and the maximum buffer size. Since the input stream
            // available() can be <= 0 we check for that and default to
            // a small msg size of 256 bytes.

            int inSize = in.available();
            if (inSize <= 0) {
               inSize = 256;
            }
            
            bufferSize = Math.min(inSize, bufferSize);
         }

         // read off all of the chunks
         StringBuffer sb = new StringBuffer(bufferSize);
         
         for (int i = 0; i < chunksToRead; i++)
         {
            sb.append(in.readUTF());
         }
         
         content = sb.toString();
      }
   }

   public void writeExternal(ObjectOutput out)
      throws IOException
   {
      super.writeExternal(out);
      
      if (content == null)
      {
         out.writeByte(NULL);
      }
      else
      {
         // apply workaround for string > 64K bug in jdk's 1.3.*

         // Split content into chunks of size 'chunkSize' and assemble
         // the pieces into a List ...

         // FIXME: could calculate the number of chunks first, then
         //        write as we chunk for efficiency
         
         ArrayList v = new ArrayList();
         int contentLength = content.length();
         
         while (contentLength > 0)
         {
            int beginCopy = (v.size()) * chunkSize;
            int endCopy = contentLength <= chunkSize ?
               beginCopy + contentLength : beginCopy + chunkSize;

            String theChunk = content.substring(beginCopy, endCopy);
            v.add(theChunk);

            contentLength -= chunkSize;
         }

         // Write out the type (OBJECT), the no. of chunks and finally
         // all chunks that have been assembled previously
         out.writeByte(OBJECT);
         out.writeInt(v.size());
         
         for (int i = 0; i < v.size(); i++)
         {
            out.writeUTF((String)v.get(i));
         }
      }
   }

   // Object override -----------------------------------------------
   public String toString() {
      return "org.jboss.mq.SpyTextMessage {\n"+
      	header+"\n"+
      	"Body {\n"+
      	"   text            :"+content+"\n"+
      	"}\n"+
      	"}";
   }
}
