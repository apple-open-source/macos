// ========================================================================
// Copyright (c) 2001 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: ServletSSL.java,v 1.15.2.3 2003/06/04 04:47:52 starksm Exp $
// ========================================================================

package org.mortbay.jetty.servlet;

/* --------------------------------------------------------------------- */
/** Jetty Servlet SSL support utilities.
 * <p> A collection of utilities required to support the SSL
 * requirements of the Servlet 2.2 and 2.3 specs.
 *
 * <p> Used by the SSL listener classes.
 *
 * @version $Id: ServletSSL.java,v 1.15.2.3 2003/06/04 04:47:52 starksm Exp $
 * @author Brett Sealey
 */
public class ServletSSL
{
    /* ------------------------------------------------------------ */
    /**
     * Given the name of a TLS/SSL cipher suite, return an int
     * representing it effective stream cipher key strength.
     * i.e. How much entropy material is in the key material being fed
     * into the encryption routines.

     * <p> This is based on the information on effective key lengths
     * in RFC 2246 - The TLS Protocol Version 1.0,
     * Appendix C. CipherSuite definitions:
     *
     * <pre>
     *                        Effective 
     *    Cipher       Type    Key Bits 
     *		       	       
     *    NULL       * Stream     0     
     *    IDEA_CBC     Block    128     
     *    RC2_CBC_40 * Block     40     
     *    RC4_40     * Stream    40     
     *    RC4_128      Stream   128     
     *    DES40_CBC  * Block     40     
     *    DES_CBC      Block     56     
     *    3DES_EDE_CBC Block    168     
     * </pre>
     * @param cipherSuite String name of the TLS cipher suite.
     * @return int indicating the effective key entropy bit-length.
     */
    public static final int deduceKeyLength(String cipherSuite)
    {
	// Roughly ordered from most common to least common.
	if (cipherSuite == null)
	    return 0;
	else if (cipherSuite.indexOf("WITH_RC4_128_") >= 0)
	    return 128;
	else if (cipherSuite.indexOf("WITH_RC4_40_") >= 0)
	    return 40;
	else if (cipherSuite.indexOf("WITH_3DES_EDE_CBC_") >= 0)
	    return 168;
	else if (cipherSuite.indexOf("WITH_IDEA_CBC_") >= 0)
	    return 128;
	else if (cipherSuite.indexOf("WITH_RC2_CBC_40_") >= 0)
	    return 40;
	else if (cipherSuite.indexOf("WITH_DES40_CBC_") >= 0)
	    return 40;
	else if (cipherSuite.indexOf("WITH_DES_CBC_") >= 0)
	    return 56;
	else
	    return 0;
    }
}
