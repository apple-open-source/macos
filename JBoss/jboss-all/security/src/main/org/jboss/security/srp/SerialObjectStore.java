/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.security.srp;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.math.BigInteger;
import java.security.KeyException;
import java.security.NoSuchAlgorithmException;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

import org.jboss.logging.Logger;
import org.jboss.security.Util;
import org.jboss.security.srp.SRPConf;
import org.jboss.security.srp.SRPVerifierStore;

/** A simple implementation of the SRPVerifierStore that uses a
file store made up of VerifierInfo serialized objects. Users and
be added or removed using the addUser and delUser methods. User passwords
are never stored in plaintext either in memory or in the serialized file.
Note that usernames and passwords are logged when a user is added
via the addUser operation. This is a development class and its use in
a production environment is not advised.

@see #addUser(String, String)
@see #delUser(String)

@author Scott.Stark@jboss.org
@version $Revision: 1.4.4.3 $
*/
public class SerialObjectStore implements SRPVerifierStore
{
    private static Logger log = Logger.getLogger(SerialObjectStore.class);
    private Map infoMap;
    private BigInteger g;
    private BigInteger N;

   /** Create an in memory store and load any VerifierInfo found in
        ./SerialObjectStore.ser if it exists.
    */
    public SerialObjectStore() throws IOException
    {
        this(null);
    }
    /** Create an in memory store and load any VerifierInfo found in
        the storeFile archive if it exists.
    */
    public SerialObjectStore(File storeFile) throws IOException
    {
        if( storeFile == null )
            storeFile = new File("SerialObjectStore.ser");
        if( storeFile.exists() == true )
        {
            FileInputStream fis = new FileInputStream(storeFile);
            ObjectInputStream ois = new ObjectInputStream(fis);
            try
            {
                infoMap = (Map) ois.readObject();
            }
            catch(ClassNotFoundException e)
            {
            }
            ois.close();
            fis.close();
        }
        else
        {
            infoMap = Collections.synchronizedMap(new HashMap());
        }

        try
        {
            Util.init();
        }
        catch(NoSuchAlgorithmException e)
        {
            e.printStackTrace();
            throw new IOException("Failed to initialzed security utils: "+e.getMessage());
        }
        N = SRPConf.getDefaultParams().N();
        g = SRPConf.getDefaultParams().g();
        log.trace("N: "+Util.tob64(N.toByteArray()));
        log.trace("g: "+Util.tob64(g.toByteArray()));
        byte[] hn = Util.newDigest().digest(N.toByteArray());
        log.trace("H(N): "+Util.tob64(hn));
        byte[] hg = Util.newDigest().digest(g.toByteArray());
        log.trace("H(g): "+Util.tob64(hg));
    }

// --- Begin SRPVerifierStore interface methods
    public VerifierInfo getUserVerifier(String username) throws KeyException, IOException
    {
        VerifierInfo info = null;
        if( infoMap != null )
            info = (VerifierInfo) infoMap.get(username);
        if( info == null )
            throw new KeyException("username: "+username+" not found");
        return info;
    }
    public void setUserVerifier(String username, VerifierInfo info)
    {
        infoMap.put(username, info);
    }

   public void verifyUserChallenge(String username, Object auxChallenge)
         throws SecurityException
   {
      throw new SecurityException("verifyUserChallenge not supported");
   }
// --- End SRPVerifierStore interface methods

    /** Save the current in memory map of VerifierInfo to the indicated
        storeFile by simply serializing the map to the file.
    */
    public void save(File storeFile) throws IOException
    {
        FileOutputStream fos = new FileOutputStream(storeFile);
        ObjectOutputStream oos = new ObjectOutputStream(fos);
        synchronized( infoMap )
        {
            oos.writeObject(infoMap);
        }
        oos.close();
        fos.close();
    }

    public void addUser(String username, String password)
    {
        log.trace("addUser, username='"+username+"', password='"+password+"'");
        VerifierInfo info = new VerifierInfo();
        info.username = username;
        /*
        long r = Util.nextLong();
        String rs = Long.toHexString(r);
         */
        String rs = "123456";
        info.salt = rs.getBytes();
        try
        {
           char[] pass = password.toCharArray();
           info.verifier = Util.calculateVerifier(username, pass,
               info.salt, N, g);
           info.g = g.toByteArray();
           info.N = N.toByteArray();
           if( log.isTraceEnabled() )
           {
               log.trace("N: "+Util.tob64(info.N));
               log.trace("g: "+Util.tob64(info.g));
               log.trace("s: "+Util.tob64(info.salt));
               byte[] xb = Util.calculatePasswordHash(username, pass, info.salt);
               log.trace("x: "+Util.tob64(xb));
               log.trace("v: "+Util.tob64(info.verifier));
               byte[] hn = Util.newDigest().digest(info.N);
               log.trace("H(N): "+Util.tob64(hn));
               byte[] hg = Util.newDigest().digest(info.g);
               log.trace("H(g): "+Util.tob64(hg));
           }
        }
        catch(Throwable t)
        {
           log.error("Failed to calculate verifier", t);
           return;
        }

        setUserVerifier(username, info);
    }
    public void delUser(String username)
    {
        infoMap.remove(username);
    }

    public static void main(String[] args) throws IOException
    {
        File storeFile = new File("SerialObjectStore.ser");
        SerialObjectStore store = new SerialObjectStore();

        for(int a = 0; a < args.length; a ++)
        {
            if( args[a].startsWith("-a") )
            {
                store.addUser(args[a+1], args[a+2]);
            }
            else if( args[a].startsWith("-d") )
            {
                store.delUser(args[a+1]);
            }
        }
        store.save(storeFile);
    }
}
