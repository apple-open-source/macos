
package org.jboss.mq.il.uil.multiplexor;

/** A circular byte[] buffer capable of expanding its capacity
 *
 * @author Scott.Stark@jboss.org
 */
public class DynCircularBuffer
{
   byte[] buffer;
   int pos;
   int end;
   boolean isFull;

   /** Creates a new instance of DynCircularBuffer */
   public DynCircularBuffer()
   {
      this(1024);
   }
   public DynCircularBuffer(int capacity)
   {
      buffer = new byte[capacity];
      pos = 0;
      end = 0;
   }

   public synchronized void clear()
   {
      pos = 0;
      end = 0;
      isFull = false;
   }

   public synchronized void fill(byte[] data)
   {
      fill(data, data.length);
   }
   public synchronized void fill(byte[] data, int length)
   {
      int freeSpace = getFreeSpace();
      int currentLength = getSize();
      if( freeSpace < length )
      {
         int newSize = buffer.length + length - freeSpace;
         byte[] tmp = new byte[newSize];
         for(int i = 0; i < currentLength; i ++)
         {
            tmp[i] = buffer[pos];
            pos ++;
            if( pos == buffer.length )
               pos = 0;
         }
         buffer = tmp;
         pos = 0;
         end = currentLength;
      }
      // Copy the data in
      for(int i = 0; i < length; i ++)
      {
         buffer[end] = data[i];
         end ++;
         if( end == buffer.length )
            end = 0;
      }
      if( end == pos )
         isFull = true;
   }

   public synchronized int get()
   {
      if( pos == end && isFull == false )
         return -1;

      int b = buffer[pos] & 0xff;
      pos ++;
      if( pos == buffer.length )
         pos = 0;
      isFull = false;
      return b;
   }

   public synchronized int get(byte b[], int off, int length)
   {
      if( pos == end && isFull == false )
         return -1;

      int getSize = Math.min(getSize(), length);
      for(int i = 0; i < getSize; i ++)
      {
         b[off + i] = buffer[pos];
         pos ++;
         if( pos == buffer.length )
            pos = 0;
      }
      isFull = isFull ? getSize == 0 : false;
      return getSize;
   }

   /** Get the amount of used space in the buffer
    */
   public synchronized int getSize()
   {
      int length = buffer.length - getFreeSpace();
      return length;
   }
   /** Get the amount of free space left in the buffer
    */
   public synchronized int getFreeSpace()
   {
      int available = 0;
      if( pos == end )
      {
         if( isFull == true )
            available = 0;
         else
            available = buffer.length;
      }
      else if( pos < end )
         available = buffer.length - end + pos;
      else
         available = pos - end;
      return available;
   }

   /** Unit test driver for the buffer
    */
   public static void main(String[] args) throws Exception
   {
      DynCircularBuffer dcb = new DynCircularBuffer(16);
      if( dcb.getFreeSpace() != 16 )
         throw new IllegalStateException("dcb.getFreeSpace() != 16");
      if( dcb.getSize() != 0 )
         throw new IllegalStateException("dcb.getSize() != 0");
      byte[] tst = {'1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
      dcb.fill(tst);
      if( dcb.getFreeSpace() != 1 )
         throw new IllegalStateException("dcb.getFreeSpace() != 1, "+dcb.getFreeSpace());
      if( dcb.getSize() != 15 )
         throw new IllegalStateException("dcb.getSize() != 15");

      byte[] tst2 = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
      dcb.clear();
      dcb.fill(tst2);
      if( dcb.getFreeSpace() != 0 )
         throw new IllegalStateException("dcb.getFreeSpace() != 0, "+dcb.getFreeSpace());
      if( dcb.getSize() != 16 )
         throw new IllegalStateException("dcb.getSize() != 16");

      dcb.clear();
      dcb.fill(tst);
      dcb.fill(tst2);
      if( dcb.getFreeSpace() != 0 )
         throw new IllegalStateException("dcb.getFreeSpace() != 0, "+dcb.getFreeSpace());
      if( dcb.getSize() != 31 )
         throw new IllegalStateException("dcb.getSize() != 31, "+dcb.getSize());

      for(int i = 0; i < tst.length; i ++)
      {
         int b = dcb.get();
         if( b != tst[i] )
            throw new IllegalStateException("tst["+i+","+tst[i]+"] != "+b);
      }
      for(int i = 0; i < tst2.length; i ++)
      {
         int b = dcb.get();
         if( b != tst2[i] )
            throw new IllegalStateException("tst2["+i+","+tst2[i]+"] != "+b);
      }
      if( dcb.get() != -1 )
         throw new IllegalStateException("dcb.get() != -1");

      dcb.fill(tst);
      byte[] out = new byte[128];
      int bytes = dcb.get(out, 0, 128);
      if( bytes != 15 )
         throw new IllegalStateException("dcb.get(out, 0, 128) != 15, "+bytes);
      if( dcb.getFreeSpace() != 31 )
         throw new IllegalStateException("dcb.getFreeSpace() != 31, "+dcb.getFreeSpace());
      if( dcb.getSize() != 0 )
         throw new IllegalStateException("dcb.getSize() != 0, "+dcb.getSize());

      dcb.clear();
      for(int n = 0; n < 32; n ++)
      {
         dcb.fill(tst);
         if( dcb.getFreeSpace() != 16 )
            throw new IllegalStateException(n+", dcb.getFreeSpace() != 16, "+dcb.getFreeSpace());
         if( dcb.getSize() != 15 )
            throw new IllegalStateException(n+"dcb.getSize() != 15");
         for(int i = 0; i < tst.length; i ++)
         {
            int b = dcb.get();
            if( b != tst[i] )
               throw new IllegalStateException(n+"tst["+i+","+tst[i]+"] != "+b);
         }
         if( dcb.get() != -1 )
            throw new IllegalStateException(n+"dcb.get() != -1");
         if( dcb.getFreeSpace() != 31 )
            throw new IllegalStateException(n+", dcb.getFreeSpace() != 31, "+dcb.getFreeSpace());
      }

      byte[] tst3 = new byte[tst.length + tst2.length];
      System.arraycopy(tst, 0, tst3, 0, tst.length);
      System.arraycopy(tst2, 0, tst3, tst.length, tst2.length);
      dcb.clear();
      dcb.fill(tst3);
      if( dcb.getFreeSpace() != 0 )
         throw new IllegalStateException("dcb.getFreeSpace() != 0, "+dcb.getFreeSpace());
      if( dcb.getSize() != 31 )
         throw new IllegalStateException("dcb.getSize() != 31, "+dcb.getSize());
      dcb.fill(tst);
      if( dcb.getFreeSpace() != 0 )
         throw new IllegalStateException("dcb.getFreeSpace() != 0, "+dcb.getFreeSpace());
      if( dcb.getSize() != 46 )
         throw new IllegalStateException("dcb.getSize() != 46, "+dcb.getSize());
      for(int i = 0; i < tst.length; i ++)
      {
         int b = dcb.get();
         if( b != tst[i] )
            throw new IllegalStateException("tst["+i+","+tst[i]+"] != "+b);
      }
      for(int i = 0; i < tst2.length; i ++)
      {
         int b = dcb.get();
         if( b != tst2[i] )
            throw new IllegalStateException("tst2["+i+","+tst2[i]+"] != "+b);
      }
      for(int i = 0; i < tst.length; i ++)
      {
         int b = dcb.get();
         if( b != tst[i] )
            throw new IllegalStateException("tst["+i+","+tst[i]+"] != "+b);
      }

      System.out.println("Done");
   }
}
