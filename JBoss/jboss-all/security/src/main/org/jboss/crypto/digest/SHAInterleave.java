package org.jboss.crypto.digest;

import java.io.ByteArrayOutputStream;
import java.security.MessageDigest;
import java.security.MessageDigestSpi;
import java.security.NoSuchAlgorithmException;
import java.security.ProviderException;

/** The SHA_Interleave algorithm as described in section 3.1 of RFC2945. This
 needs an SHA MessageDigest provider to function.

@author Scott.Stark@jboss.org
@version $Revision: 1.1 $
*/
public class SHAInterleave extends MessageDigestSpi
{
   private static final int SHA_HASH_LEN = 20;

   private ByteArrayOutputStream evenBytes;
   private ByteArrayOutputStream oddBytes;
   private int count;
   private boolean skipLeadingZeros;
   private MessageDigest sha;

   /** Creates a new instance of SHAInterleave
    @exception ProviderException thrown if MessageDigest.getInstance("SHA")
    throws a NoSuchAlgorithmException.
    */
   public SHAInterleave()
   {
      try
      {
         sha = MessageDigest.getInstance("SHA");
      }
      catch(NoSuchAlgorithmException e)
      {
         throw new ProviderException("Failed to obtain SHA MessageDigest");
      }
      evenBytes = new ByteArrayOutputStream();
      oddBytes = new ByteArrayOutputStream();
      engineReset();
   }

   protected int engineGetDigestLength()
   {
      return 2 * SHA_HASH_LEN;
   }

   /**
    * Completes the digest computation by performing final
    * operations such as padding. Once <code>engineDigest</code> has
    * been called, the engine should be reset (see
    * {@link #engineReset() engineReset}).
    * Resetting is the responsibility of the
    * engine implementor.
    *
    * @return the array of bytes for the resulting digest value.
    */
   protected byte[] engineDigest()
   {
      byte[] E = evenBytes.toByteArray();
      byte[] G = sha.digest(E);
      // If the count is odd, drop the first byte
      byte[] F = oddBytes.toByteArray();
      int offset = 0;
      if( count % 2 == 1 )
         offset = 1;
      sha.reset();
      sha.update(F, offset, F.length-offset);
      byte[] H = sha.digest();
      int length = G.length + H.length;
      byte[] digest = new byte[length];
      for(int i = 0; i < G.length; ++i)
         digest[2 * i] = G[i];
      for(int i = 0; i < H.length; ++i)
         digest[2 * i + 1] = H[i];
      engineReset();
      return digest;
   }

   /**
    * Resets the digest for further use.
    */
   protected void engineReset()
   {
      skipLeadingZeros = true;
      count = 0;
      evenBytes.reset();
      oddBytes.reset();
      sha.reset();
   }

   /**
    * Updates the digest using the specified byte.
    *
    * @param input the byte to use for the update.
    */
   protected void engineUpdate(byte input)
   {
      if( skipLeadingZeros == true && input == 0 )
         return;
      skipLeadingZeros = false;
      if( count % 2 == 0 )
         evenBytes.write(input);
      else
         oddBytes.write(input);
      count ++;
   }

   /**
    * Updates the digest using the specified array of bytes,
    * starting at the specified offset.
    *
    * @param input the array of bytes to use for the update.
    * @param offset the offset to start from in the array of bytes.
    * @param len the input of bytes to use, starting at
    * <code>offset</code>.
    */
   protected void engineUpdate(byte[] input, int offset, int len)
   {
      for(int i = offset; i < offset+len; i ++)
         engineUpdate(input[i]);
   }

}
