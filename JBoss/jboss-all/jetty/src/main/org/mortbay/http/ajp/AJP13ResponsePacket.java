package org.mortbay.http.ajp;

/** AJP13ResponsePacket used by AJP13OutputStream
 * @author Jason Jenkins <jj@aol.net>
 *
 * This class has the HTTP head encodings for AJP13 Response Packets 
 */
public class AJP13ResponsePacket extends AJP13Packet {
	
	public static String[] __ResponseHeader=
	   {
	   	"ERROR",
	"Content-Type", 
    "Content-Language", 
    "Content-Length", 
    "Date", 
    "Last-Modified", 
    "Location", 
    "Set-Cookie", 
    "Set-Cookie2", 
    "Servlet-Engine", 
    "Status", 
    "WWW-Authenticate"
	   };

	/**
	 * @param buffer
	 * @param len
	 */
	public AJP13ResponsePacket(byte[] buffer, int len) {
		
		super(buffer, len);
		
	}

	/**
	 * @param buffer
	 */
	public AJP13ResponsePacket(byte[] buffer) {
		super(buffer);
		
	}

	/**
	 * @param size
	 */
	public AJP13ResponsePacket(int size) {
		super(size);
		
	}
	
	public void populateHeaders() {
		__header = __ResponseHeader;
		for (int  i=1;i<__ResponseHeader.length;i++)
			__headerMap.put(__ResponseHeader[i],new Integer(0xA000+i));
	}
	}

