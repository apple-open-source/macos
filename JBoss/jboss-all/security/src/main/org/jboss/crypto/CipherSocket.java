package org.jboss.crypto;

import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.net.Socket;
import java.util.Arrays;
import javax.crypto.Cipher;
import javax.crypto.CipherInputStream;
import javax.crypto.CipherOutputStream;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.SecretKey;

/**
 *
 * @author  Scott.Stark@jboss.org
 */
public class CipherSocket extends Socket
{
   private Cipher cipher;
   private Socket delegate;
   String algorithm;
   SecretKey key;

   /** Creates a new instance of CipherSocket */
   public CipherSocket(String host, int port, String algorithm, SecretKey key)
      throws IOException
   {
      super(host, port);
      this.algorithm = algorithm;
      this.key = key;
   }
   public CipherSocket(Socket delegate, String algorithm, SecretKey key)
      throws IOException
   {
      this.delegate = delegate;
      this.algorithm = algorithm;
      this.key = key;
   }

   public InputStream getInputStream() throws IOException
   {
      InputStream is = delegate == null ? super.getInputStream() : delegate.getInputStream();
      Cipher cipher = null;
      try
      {
         cipher = Cipher.getInstance(algorithm);
         int size = cipher.getBlockSize();
         byte[] tmp = new byte[size];
         Arrays.fill(tmp, (byte)15);
         IvParameterSpec iv = new IvParameterSpec(tmp);
         cipher.init(Cipher.DECRYPT_MODE, key, iv);
      }
      catch(Exception e)
      {
         e.printStackTrace();
         throw new IOException("Failed to init cipher: "+e.getMessage());
      }
      CipherInputStream cis = new CipherInputStream(is, cipher);
      return cis;
   }

   public OutputStream getOutputStream() throws IOException
   {
      OutputStream os = delegate == null ? super.getOutputStream() : delegate.getOutputStream();
      Cipher cipher = null;
      try
      {
         cipher = Cipher.getInstance(algorithm);
         int size = cipher.getBlockSize();
         byte[] tmp = new byte[size];
         Arrays.fill(tmp, (byte)15);
         IvParameterSpec iv = new IvParameterSpec(tmp);
         cipher.init(Cipher.ENCRYPT_MODE, key, iv);
      }
      catch(Exception e)
      {
         throw new IOException("Failed to init cipher: "+e.getMessage());
      }
      CipherOutputStream cos = new CipherOutputStream(os, cipher);
      return cos;
   }
}
