// ===========================================================================
// Copyright (c) 2002 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: ByteArrayPool.java,v 1.3.2.5 2003/06/04 04:47:58 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.util;


/* ------------------------------------------------------------ */
/** Byte Array Pool
 * Simple pool for recycling byte arrays of a fixed size.
 *
 * @version $Id: ByteArrayPool.java,v 1.3.2.5 2003/06/04 04:47:58 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
public class ByteArrayPool
{
    public static final int __POOL_SIZE=
        Integer.getInteger("org.mortbay.util.ByteArrayPool.pool_size",8).intValue();
    
    public static final ThreadLocal __pools=new BAThreadLocal();
    public static int __slot;
    
    /* ------------------------------------------------------------ */
    /** Get a byte array from the pool of known size.
     * @param size Size of the byte array.
     * @return Byte array of known size.
     */
    public static byte[] getByteArray(int size)
    {
        byte[][] pool = (byte[][])__pools.get();
        for (int i=pool.length;i-->0;)
        {
            if (pool[i]!=null && pool[i].length==size)
            {
                byte[]b = pool[i];
                pool[i]=null;
                return b;
            }
        }
        return new byte[size];
    }

    /* ------------------------------------------------------------ */
    public static void returnByteArray(final byte[] b)
    {
        if (b==null)
            return;
        
        byte[][] pool = (byte[][])__pools.get();
        for (int i=pool.length;i-->0;)
        {
            if (pool[i]==null)
            {
                pool[i]=b;
                return;
            }
        }

        // slot.
        int s = __slot++;
        if (s<0)s=-s;
        pool[s%pool.length]=b;
    }

    
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    private static final class BAThreadLocal extends ThreadLocal
    {
        protected Object initialValue()
            {
                return new byte[__POOL_SIZE][];
            }
    }
}
