package CyrusSasl;

import javax.security.auth.callback.*;
import java.io.*;

public class GenericClient extends GenericCommon implements SaslClient 
{

    private byte[]initial_response;
    private String mechanism;
    private javax.security.auth.callback.CallbackHandler cbh;

    GenericClient(int cptr, String mechlist,
		  java.util.Hashtable props,
		  javax.security.auth.callback.CallbackHandler cbh)
    {
	ptr=cptr;
	this.cbh = cbh;

	/* set properties */
	super.setcommonproperties(props);

	initial_response=jni_sasl_client_start(cptr, mechlist);	
    }

    private native byte[] jni_sasl_client_start(int ptr,
						String mechlist);

    /**
     * Perform a step. start() must have been preformed succesfully
     * before this step() can be called. A client should call this
     * method until it receives notification from the server that
     * authentication is complete. Any protocol specific decoding (such
     * as base64 decoding) must be done before calling step(). The
     * return byte array should be encoded by the protocol specific
     * method then sent to the server
     *
     * @param challenge byte[] from server (must be protocol specific decode before)
     * @exception saslException sasl exception
     * @return the byte[] you should send to the server */
    
    public byte[] evaluateChallenge(byte[] challenge) throws SaslException
    {
	byte[] out=null;

	if (challenge==null) {
	    out=jni_sasl_client_step(ptr, null, 0);
	} else {
	    out=jni_sasl_client_step(ptr, challenge, challenge.length);
	}
	
	return out;
    }

    private native byte[] jni_sasl_client_step(int ptr,
					       byte[] in,
					       int inlen);


    public boolean hasInitialResponse()
    {
	/* xxx */
	return false;
    }
	
    /**
     * Use this method to obtain the name of the mechanism being
     * negotiated with the server. After giving start() a list of
     * mechanisms one will be chosen. Use this method to determine which
     * one if being used if any.
     *
     * @return the mechanism currently negotiated or already negotiated */

    public String getMechanismName()
    {
	return mechanism;
    }

    /* called from C layer */
    private void callback_setmechanism(String mech)
    {
	mechanism=mech;
    }

    private String userid;
    private String authid;
    private String password;
    private String realm;

    private void do_callbacks(int wantuid, int wantaid, int wantpass, int wantrealm) throws SaslException
    {
	int numcb = wantuid+wantaid+wantpass+wantrealm;

	Callback[] cbs = new Callback[numcb];
	int pos = 0;

	NameCallback nc = null;
	NameCallback nc2 = null;
	PasswordCallback pc = null;
	RealmCallback rc = null;

	if (wantuid==1) {
	    nc = new NameCallback("Please enter your userid");
	    cbs[pos] = nc;
	    pos++;
	}

	if (wantaid==1) {
	    nc2 = new NameCallback("Please enter your authid");
	    cbs[pos] = nc2;
	    pos++;
	}

	if (wantpass==1) {
	    pc = new PasswordCallback("Please enter your password");
	    cbs[pos] = pc;
	    pos++;
	}

	if (wantrealm==1) {
	    rc = new RealmCallback("Please enter your realm");
	    cbs[pos] = rc;
	    pos++;
	}
	
	try {
	    cbh.invokeCallback(cbs);
	} catch (UnsupportedCallbackException e) {
	    throw new SaslException("Unsupported callback",null);
	} catch (IOException e) {
	    throw new SaslException("IO exception",null);
	}

	if (nc!=null) {
	    this.userid = nc.getName();
	}
	if (nc2!=null) {
	    this.authid = nc2.getName();
	}
	if (pc!=null) {
	    this.password = pc.getPassword();
	}
	if (rc!=null) {
	    this.realm = rc.getRealm();
	}
    }

    private String get_userid(int a)
    {
	return userid;
    }
    private String get_authid(int a)
    {
	return authid;
    }
    private String get_password(int a)
    {
	return password;
    }
    private String get_realm(int a)
    {
	return realm;
    }

    public InputStream getInputStream(InputStream source) throws IOException
    {
	if (getSecurity() > 0) {
	    return new SaslInputStream(source,this);
	} else {
	    // no security layer, no indirection needed
	    return source;
	}
    }

    public OutputStream getOutputStream(OutputStream dest) throws IOException
    {
	if (getSecurity() > 0) {
	    return new SaslOutputStream(dest,this);
	} else {
	    // no security layer, no indirection needed
	    return dest;
	}
    }

    public byte[] createInitialResponse(){
	return initial_response;
    }
}
