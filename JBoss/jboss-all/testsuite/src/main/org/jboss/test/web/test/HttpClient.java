package org.jboss.test.web.test;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.net.Socket;
import java.net.URL;
import java.util.HashMap;

// Unpublished Base64 encoder class
import sun.misc.BASE64Encoder;

/** A simple socket based client for accesing http content.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.6 $
 */
public class HttpClient
{
   URL reqURL;
   int responseCode = -1;
   String response;
   StringBuffer content;
   HashMap headers = new HashMap();

   /** Creates new HttpClient */
   public HttpClient(String url) throws IOException
   {
      this.reqURL = new URL(url);
   }
   public HttpClient(URL url) throws IOException
   {
      this.reqURL = url;
   }

   public int getResponseCode() throws IOException
   {
      if( responseCode < 0 )
         transfer();
      return responseCode;
   }
   public String getResponseMessage() throws IOException
   {
      if( responseCode < 0 )
         transfer();
      return response;
   }
   public StringBuffer getContent() throws IOException
   {
      if( responseCode < 0 )
         transfer();
      return content;
   }

   public int transfer() throws IOException
   {
      String host = reqURL.getHost();
      int port = reqURL.getPort();
      if( port < 0 )
         port = 80;
      Socket conn = new Socket(host, port);
      OutputStream os = conn.getOutputStream();
      writeHeader(os);
      InputStream is = conn.getInputStream();
      readHeader(is);
      os.close();
      is.close();
      return responseCode;
   }

   private void writeHeader(OutputStream os)
   {
      PrintWriter pw = new PrintWriter(os);
      pw.write("GET " + reqURL.getPath() + " HTTP/1.1\r\n");
      pw.write("Accept: */*\r\n");
      pw.write("User-Agent: JBossTest WebClient\r\n");
      pw.write("Host: localhost:8080\r\n");
      pw.write("Connection: close\r\n");
      String userInfo = reqURL.getUserInfo();
      if( userInfo != null )
      {
         BASE64Encoder encoder = new BASE64Encoder();
         byte[] userInfoBytes = userInfo.getBytes();
         String authInfo = "Basic " + encoder.encode(userInfoBytes);
         pw.write("Authorization: " + authInfo+"\r\n");
      }
      pw.write("\r\n");
      pw.flush();
   }

   private void readHeader(InputStream is) throws IOException
   {
      BufferedReader reader = new BufferedReader(new InputStreamReader(is));
      String line;
      while( (line = reader.readLine()) != null )
      {
         // Check for "\r\n\r\n" end of headers marker
         if( line.length() == 0 )
            break;
         
         if( responseCode < 0 )
         {
            // Parse the intial line for the "HTTP/1.x 200 OK" response
            System.out.println("HttpClient.reponse = "+line);
            response = line.substring(13);
            line = line.substring(9, 12);
            responseCode = Integer.parseInt(line);
         }
         else
         {
            int separator = line.indexOf(':');
            if( separator > 0 )
            {
               String key = line.substring(0, separator);
               String value = line.substring(separator+1);
               headers.put(key, value);
            }
            else
            {
               System.out.println("Invalid header '"+line+"'");
            }
         }
      }
      content = new StringBuffer();
      while( (line = reader.readLine()) != null )
      {
	//         if( line.length() == 0 )
	//            break;
         content.append(line);
         content.append('\n');
      }
   }

   public static void main(String[] args) throws IOException
   {
      HttpClient hc = new HttpClient("http://jduke:theduke@localhost:8080/jbosstest/restricted/SecureServlet");
      System.out.println(hc.transfer());
   }
}
