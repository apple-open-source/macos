// ========================================================================
// Copyright (c) 1998 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: Credential.java,v 1.1.4.3 2003/06/04 04:47:58 starksm Exp $
// ========================================================================

package org.mortbay.util;

import java.security.MessageDigest;


/* ------------------------------------------------------------ */
/** Credentials.
 * The Credential class represents an abstract mechanism for checking
 * authentication credentials.  A credential instance either represents a
 * secret, or some data that could only be derived from knowing the secret.
 * <p>
 * Often a Credential is related to a Password via a one way algorithm, so
 * while a Password itself is a Credential, a UnixCrypt or MD5 digest of a
 * a password is only a credential that can be checked against the password.
 * <p>
 * This class includes an implementation for unix Crypt an MD5 digest. 
 * @see Password
 * @version $Id: Credential.java,v 1.1.4.3 2003/06/04 04:47:58 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
public abstract class Credential
{
    /* ------------------------------------------------------------ */
    /** Check a credential
     * @param credentials The credential to check against. This may either be
     * another Credential object, a Password object or a String which is
     * interpreted by this credential. 
     * @return True if the credentials indicated that the shared secret is
     * known to both this Credential and the passed credential.
     */
    public abstract boolean check(Object credentials);

    /* ------------------------------------------------------------ */
    /** Get a credential from a String.
     * If the credential String starts with a known Credential type (eg
     * "CRYPT:" or "MD5:" ) then a Credential of that type is returned. Else the
     * credential is assumed to be a Password.
     * @param credential String representation of the credential
     * @return A Credential or Password instance.
     */
    public static Credential getCredential(String credential)
    {
        if (credential.startsWith(Crypt.__TYPE))
            return new Crypt(credential);
        if (credential.startsWith(MD5.__TYPE))
            return new MD5(credential);
        
        return new Password(credential);
    }


    /* ------------------------------------------------------------ */
    /** Unix Crypt Credentials
     */
    public static class Crypt extends Credential
    {
        public static final String __TYPE="CRYPT:";
        
        private String _cooked;
        Crypt(String cooked)
        {
            _cooked=cooked.startsWith(Crypt.__TYPE)
                ?cooked.substring(__TYPE.length())
                :cooked;
        }
        
        public boolean check(Object credentials)
        {
            if (!(credentials instanceof String) &&
                !(credentials instanceof Password))
                Code.warning("Can't check "+credentials.getClass()+" against CRYPT");
            
            String passwd = credentials.toString();
            return _cooked.equals(UnixCrypt.crypt(passwd,_cooked));
        }

        public static String crypt(String user,String pw)
        {
            return "CRYPT:"+UnixCrypt.crypt(pw,user);
        }
    }
    
    /* ------------------------------------------------------------ */
    /** Unix Crypt Credentials
     */
    public static class MD5 extends Credential
    {
        public static final String __TYPE="MD5:";
        private static MessageDigest __md;
        
        private byte[] _digest;
        
        /* ------------------------------------------------------------ */
        MD5(String digest)
        {
            digest=digest.startsWith(__TYPE)
                ?digest.substring(__TYPE.length())
                :digest;
            _digest=TypeUtil.parseBytes(digest,16);
        }
        
        /* ------------------------------------------------------------ */
        public boolean check(Object credentials)
        {
            try
            {
                byte[] digest=null;
                
                if (credentials instanceof Password ||
                    credentials instanceof String)
                {
                    synchronized(__TYPE)
                    {
                        if (__md==null)
                            __md = MessageDigest.getInstance("MD5");
                        __md.reset();
                        __md.update(credentials.toString().getBytes(StringUtil.__ISO_8859_1));
                        digest=__md.digest();
                    }
                }
                else
                    Code.warning("Can't check "+credentials.getClass()+" against MD5");
                
                if (digest==null || digest.length!=_digest.length)
                    return false;
                for (int i=0;i<digest.length;i++)
                    if (digest[i]!=_digest[i])
                        return false;
                return true;
            }
            catch (Exception e)
            {
                Code.warning(e);
                return false;
            }
        }

        /* ------------------------------------------------------------ */
        public static String digest(String password)
        {
            try
            {
                byte[] digest;
                synchronized(__TYPE)
                {
                    if (__md==null)
                    {
                        try{__md = MessageDigest.getInstance("MD5");}
                        catch (Exception e ) {Code.warning(e);return null;}
                    }
                    
                    __md.reset();
                    __md.update(password.getBytes(StringUtil.__ISO_8859_1));
                    digest=__md.digest();
                }
                
                return __TYPE+TypeUtil.toString(digest,16);
            }
            catch (Exception e)
            {
                Code.warning(e);
                return null;
            }
        }
    }
}
