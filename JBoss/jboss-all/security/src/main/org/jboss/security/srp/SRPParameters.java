/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.srp;

import java.io.Serializable;
import java.util.Arrays;

import org.jboss.security.Util;

/** The RFC2945 algorithm session parameters that the client and server
agree to use. In addition to the base RFC2945 parameters, one can choose an
alternate hash algorithm for the private session key.

@author Scott.Stark@jboss.org
@version $Revision: 1.2.6.2 $
*/
public class SRPParameters implements Cloneable, Serializable
{
   /** The serial version ID.
    * @since 1.2.4.1
    */
   private static final long serialVersionUID = 6438772808805276693L;

   /** The algorithm safe-prime modulus */
   public final byte[] N;
   /** The algorithm primitive generator */
   public final byte[] g;
   /** The random password salt originally used to verify the password */
   public final byte[] s;
   /** The algorithm to hash the session key to produce K. To be consistent
    with the RFC2945 description this must be SHA_Interleave as implemented
    by the JBossSX security provider. For compatibility with earlier JBossSX
    SRP releases the algorithm must be SHA_ReverseInterleave. This name is
    passed to java.security.MessageDigest.getInstance(). */
   public final String hashAlgorithm;
   /** The algorithm to use for any encryption of data.
    */
   public final String cipherAlgorithm;
   /** The cipher intialization vector bytes
    */
   public byte[] cipherIV;

   /** Creates a new instance of SRPParameters */
   public SRPParameters(byte[] N, byte[] g, byte[] s)
   {
      this(N, g, s, "SHA_Interleave", null);
   }
   public SRPParameters(byte[] N, byte[] g, byte[] s, String hashAlgorithm)
   {
      this(N, g, s, hashAlgorithm, null);
   }
   public SRPParameters(byte[] N, byte[] g, byte[] s, String hashAlgorithm,
      String cipherAlgorithm)
   {
      this.N = N;
      this.g = g;
      this.s = s;
      if( hashAlgorithm == null )
         hashAlgorithm = "SHA_Interleave";
      this.hashAlgorithm = hashAlgorithm;
      this.cipherAlgorithm = cipherAlgorithm;
   }

   public Object clone()
   {
      Object clone = null;
      try
      {
          clone = super.clone();
      }
      catch(CloneNotSupportedException e)
      {
      }
      return clone;
   }

   public int hashCode()
   {
      int hashCode = hashAlgorithm.hashCode();
      for(int i = 0; i < N.length; i ++)
         hashCode += N[i];
      for(int i = 0; i < g.length; i ++)
         hashCode += g[i];
      for(int i = 0; i < s.length; i ++)
         hashCode += s[i];
      return hashCode;
   }

   public boolean equals(Object obj)
   {
      boolean equals = false;
      if( obj instanceof SRPParameters )
      {
         SRPParameters p = (SRPParameters) obj;
         equals = hashAlgorithm.equals(p.hashAlgorithm);
         if( equals == true )
            equals = Arrays.equals(N, p.N);
         if( equals == true )
            equals = Arrays.equals(g, p.g);
         if( equals == true )
            equals = Arrays.equals(s, p.s);
      }
      return equals;
   }

   public String toString()
   {
      StringBuffer tmp = new StringBuffer(super.toString());
      tmp.append('{');
      tmp.append("N: ");
      tmp.append(Util.encodeBase64(N));
      tmp.append("|g: ");
      tmp.append(Util.encodeBase64(g));
      tmp.append("|s: ");
      tmp.append(Util.encodeBase64(s));
      tmp.append("|hashAlgorithm: ");
      tmp.append(hashAlgorithm);
      tmp.append("|cipherAlgorithm: ");
      tmp.append(cipherAlgorithm);
      tmp.append("|cipherIV: ");
      tmp.append(cipherIV);
      tmp.append('}');
      return tmp.toString();
   }
}
