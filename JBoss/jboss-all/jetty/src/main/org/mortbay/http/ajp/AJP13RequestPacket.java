package org.mortbay.http.ajp;

/** AJP13RequestPacket used by AJP13InputStream
 * @author Jason Jenkins <jj@aol.net>
 *
 * This class has the HTTP head encodings for AJP13 Request Packets 
 */
public class AJP13RequestPacket extends AJP13Packet {
	
	public static String[] __RequestHeader=
	   {
		   "ERROR",
		   "accept",
		   "accept-charset",
		   "accept-encoding",
		   "accept-language",
		   "authorization",
		   "connection",
		   "content-type",
		   "content-length",
		   "cookie",
		   "cookie2",
		   "host",
		   "pragma",
		   "referer",
		   "user-agent"
	   };
	   

	/**
	 * @param buffer
	 * @param len
	 */
	public AJP13RequestPacket(byte[] buffer, int len) {
		super(buffer, len);
	
	}

	/**
	 * @param buffer
	 */
	public AJP13RequestPacket(byte[] buffer) {
		super(buffer);
		
	}

	/**
	 * @param size
	 */
	public AJP13RequestPacket(int size) {
		super(size);
		
	}
	
	public void populateHeaders() {
			__header = __RequestHeader;
			for (int  i=1;i<__RequestHeader.length;i++)
				__headerMap.put(__RequestHeader[i],new Integer(0xA000+i));
		}

}
