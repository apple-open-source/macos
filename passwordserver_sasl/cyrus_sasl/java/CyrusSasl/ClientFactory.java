package CyrusSasl;

import java.util.Hashtable;
import javax.security.auth.callback.*;

class ClientFactory implements SaslClientFactory
{

    public ClientFactory()
    {

    }

    /* JNI functions  */
    private native int jni_sasl_client_init(String appname);
    private native int jni_sasl_client_new(String service,
					   String serverFQDN,
					   int secflags);


    private boolean init_client(String appname)
    {
	/* load library */
	try {
	    System.loadLibrary("javasasl");
	} catch (UnsatisfiedLinkError e) {
	    /* xxx */
	    System.out.println("Unable to load javasasl library");
	}

	jni_sasl_client_init(appname);    
	
	return true;
    }


    private static boolean client_initialized = false;


    public SaslClient createSaslClient(String[] mechanisms,
				       String authorizationId,
				       String protocol,
				       String serverName,
				       Hashtable props,
				       javax.security.auth.callback.CallbackHandler cbh)
	throws SaslException
    {
	int cptr;

	if (client_initialized == false) {
	    /* TODO: This should only be done once, even if called in
	     * multiple threads... */
	    client_initialized = init_client("javasasl application");
	}

	cptr = jni_sasl_client_new(protocol,
				   serverName,
				   0);

	if (cptr == 0) {
	    throw new SaslException("Unable to create new Client connection object", new Throwable());
	}

	/* create the mechlist the way our library likes to see it */
	String mechlist="";

	for (int lup=0;lup<mechanisms.length;lup++) {
	    mechlist+=mechanisms[lup];
	    mechlist+=" ";
	}

	
	return new GenericClient(cptr, mechlist,props,cbh);
    }



}
