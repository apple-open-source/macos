/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.http;

import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;

import java.net.Authenticator;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;

import org.jboss.logging.Logger;

import org.jboss.security.SecurityAssociationAuthenticator;

// This class is javax.net.ssl.HttpsURLConnection in JDK1.4+
import com.sun.net.ssl.HttpsURLConnection;

/**
 * Utility class that provides HTTP functionality.  This class is modeled after
 * Scott's Util class in org.jboss.invocation.http.interfaces.Util, however,
 * that class deals with Invocation object, while this handles HTTPILRequest
 * objects.  Other then that, it is a pretty close reproduction.
 *
 * Concerning SSL, like Scott's Util class, a check is performed at run time to
 * determine if the connection is the JSSE 1.0.2 package (i.e. the connection is
 * an instance of com.sun.net.ssl.HttpsURLConnection).  If so, the system
 * properties are queried to see if the org.jboss.security.ignoreHttpsHost
 * value is set to true.  Setting this property will cause the AnyhostVerifier
 * to be installed, therefore overriding the default verification of the server
 * certificate.  This DOESN'T mean that you don't have to have the certificate
 * in your trust store--you do.  The only thing this the AnyhostVerifier--
 * as its name implies--does is to remove the requirement that the CN on the
 * server certificate match the hostname.
 *
 * I struggled with weather I should even include this mechanism as it doesn't
 * work in the implementation of JSDK 1.4 because Sun changed the class names.
 * Furthermore, I can't make a instanceof call to check for the existence of the
 * JDK 1.4 implementation (same one, just different package name) because then
 * the code wouldn't compile with the version of JSSE currently included with
 * JBoss!  So, my choices are 1.) remove the instanceof check completely and
 * require that the user deal with the problem 2.) I could keep the code in just
 * like Scott's Util class and require that only JDK 1.4 users have to deal with
 * the issue, or 3.) I could create two versions of this and the AnyhostVerifier
 * classes--one for JDK 1.3 and one for JDK 1.4, and include a check in the
 * build so that when being built with 1.4, the 1.4 versions are included and
 * when building in 1.3 the 1.3 versions are included instead.  Ultimately I
 * decided that number 2 was probably the best option.  It was consistent with
 * Scott's Util class and since these two will likely be used together I felt it
 * was important to have the same set of rules for both.  Plus, it does solve the
 * problem for some of the population who still use JDK 1.3 and JSSE 1.0.2.
 * Lastly, the 3 option--as attractive as it sounds--simply doesn't fit in with
 * our current release policies--we only release ONE version of the build--not
 * a JDK 1.4 version and an JDK 1.3 version.  So, the short of it is, if you are
 * using JDK 1.3 with JSSE 1.0.2, set the org.jboss.security.ignoreHttpsHost
 * property and your certificate CN doesn't have to match the host name.
 * If you are using JDK 1.4 or above, you'll simply have to deal with it.  If
 * you want to modify this code to do this, all you need do is: 1.) import
 * javax.net.ssl.HttpsURLConnection INSTEAD of com.sun.net.ssl.HttpsURLConnection
 * in this class, and import javax.net.ssl.HostnameVerifier INSTEAD of
 * com.sun.net.ssl.HostnameVerifier in AnyhostVerifier, and 2.) implement the
 * new verify method in AnyhostVerifier.
 *
 * @author    Nathan Phelps (nathan@jboss.org)
 * @version   $Revision: 1.1.2.1 $
 * @created   January 15, 2003
 */
public class HTTPClient
{
    
    private static final String CONTENT_TYPE = "application/x-java-serialized-object; class=org.jboss.mq.il.http.HTTPILRequest";
    
    private static Logger log = Logger.getLogger(HTTPClient.class);
    
    static
    {
        try
        {
            Authenticator.setDefault(new SecurityAssociationAuthenticator());
        }
        catch(Exception exception)
        {
            log.warn("Failed to install SecurityAssociationAuthenticator", exception);
        }
    }
    
    public static Object post(URL url, HTTPILRequest request) throws Exception
    {
        if (log.isTraceEnabled())
        {
            log.trace("post(URL " + url.toString() + ", HTTPILRequest " + request.toString() + ")");
        }
        HttpURLConnection connection = (HttpURLConnection)url.openConnection();
        if (connection instanceof HttpsURLConnection)
        {
            // See if the org.jboss.security.ignoreHttpsHost property is set
            if (Boolean.getBoolean("org.jboss.security.ignoreHttpsHost") == true)
            {
                if (log.isDebugEnabled())
                {
                    log.debug("Using the AnyhostVerifier");
                }
                HttpsURLConnection httpsConnection = (HttpsURLConnection) connection;
                AnyhostVerifier verifier = new AnyhostVerifier();
                httpsConnection.setHostnameVerifier(verifier);
            }
        }
        
        connection.setDoInput(true);
        connection.setDoOutput(true);
        connection.setUseCaches(false);
        connection.setRequestProperty("ContentType", CONTENT_TYPE);
        connection.setRequestMethod("POST");
        ObjectOutputStream outputStream = new ObjectOutputStream(connection.getOutputStream());
        outputStream.writeObject(request);
        outputStream.close();
        ObjectInputStream inputStream = new ObjectInputStream(connection.getInputStream());
        HTTPILResponse response = (HTTPILResponse)inputStream.readObject();
        inputStream.close();
        Object responseValue = response.getValue();
        if (responseValue instanceof Exception)
        {
            throw (Exception)responseValue;
        }
        return responseValue;
    }
    
    public static URL resolveServerUrl(String url) throws MalformedURLException
    {
        if (url == null)
        {
            throw new MalformedURLException("URL is null.");
        }
        try
        {
            return new URL(url);
        }
        catch (MalformedURLException exception)
        {
            String propertyValue = System.getProperty(url);
            if (propertyValue == null)
            {
                throw exception;
            }
            return new URL(propertyValue);
        }
    }
}
