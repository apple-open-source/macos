/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.proxy.ejb;

import javax.rmi.CORBA.Util;
import javax.rmi.CORBA.Stub;
import org.omg.CORBA_2_3.portable.InputStream;
import org.omg.CORBA_2_3.portable.OutputStream;
import org.omg.CORBA.portable.ApplicationException;
import org.omg.CORBA.portable.RemarshalException;
import org.omg.CORBA.SystemException;

import org.jboss.iiop.rmi.marshal.strategy.StubStrategy;
import org.jboss.logging.Logger;
import org.jboss.proxy.compiler.InvocationHandler;
import org.jboss.proxy.ejb.HandleImplIIOP;
import org.jboss.proxy.ejb.HomeHandleImplIIOP;

/**
 * Dynamically generated IIOP stub classes extend this abstract superclass,
 * which extends <code>javax.rmi.CORBA.Stub</code>.
 *
 * A <code>DynamicIIOPStub</code> is a local proxy of a remote object. It has 
 * methods (<code>invoke()</code>, <code>invokeBoolean()</code>, 
 * <code>invokeByte()</code>, and so on) that send an IIOP request to the 
 * server that implements the remote object, receive the reply from the
 * server, and return the results to the caller. All of these methods take
 * the IDL name of the operation, a <code>StubStrategy</code> instance to 
 * be used for marshalling parameters and unmarshalling the result, plus an 
 * array of operation parameters.
 * 
 * @author  <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @version $Revision: 1.1.2.3 $
 */
public abstract class DynamicIIOPStub 
      extends javax.rmi.CORBA.Stub
{

   // Attributes -------------------------------------------------------------
   
   /**
    * My handle (either a HandleImplIIOP or a HomeHandleImplIIOP).
    */
   private Object handle = null;

   // Static ------------------------------------------------------------------

   private static final Logger logger = 
                           Logger.getLogger(DynamicIIOPStub.class);

   private static void trace(String msg)
   {
      if (logger.isTraceEnabled()) 
         logger.trace(msg);
   }

   // Constructor -------------------------------------------------------------

   /**
    * Constructs a <code>DynamicIIOPStub</code>.
    */
   public DynamicIIOPStub()
   {
      super();
   }

   // Methods used by dynamically generated IIOP stubs ------------------------

   /**
    * Sends a request message to the server, receives the reply from the 
    * server, and returns an <code>Object</code> result to the caller.
    */
   public Object invoke(String operationName, 
                        StubStrategy stubStrategy, Object[] params)
         throws Throwable
   {
      if (operationName.equals("_get_handle") 
          && this instanceof javax.ejb.EJBObject) {
         if (handle == null) {
            handle = new HandleImplIIOP(this);
         }
         return handle;
      }
      else if (operationName.equals("_get_homeHandle")
               && this instanceof javax.ejb.EJBHome) {
         if (handle == null) {
            handle = new HomeHandleImplIIOP(this);
         }
         return handle;
      }
      else if (!_is_local()) { 
         // remote call path
         
         // To check whether this is a local stub or not we must call
         // org.omg.CORBA.portable.ObjectImpl._is_local(), and _not_ 
         // javax.rmi.CORBA.Util.isLocal(Stub s), which in Sun's JDK 
         // always return false.

         InputStream in = null;
         try {
            try {
               OutputStream out = 
                  (OutputStream)_request(operationName, true);
               stubStrategy.writeParams(out, params);
               trace("sent request: " + operationName);
               in = (InputStream)_invoke(out);
               if (stubStrategy.isNonVoid()) {
                  trace("received reply");
                  return stubStrategy.readRetval(in);
                  //Object retval = stubStrategy.readRetval(in);
                  //trace("retval: " + retval);
                  //return retval;
               }
               else
                  return null;
            }
            catch (ApplicationException ex) {
               trace("got application exception");
               in =(InputStream)ex.getInputStream();
               throw stubStrategy.readException(ex.getId(), in);
            }
            catch (RemarshalException ex) {
               trace("got remarshal exception");
               return invoke(operationName, stubStrategy, params);
            }
         }
         catch (SystemException ex) {
            if (logger.isTraceEnabled()) {
               logger.trace("CORBA system exception in IIOP stub", ex);
            }
            throw Util.mapSystemException(ex);
         }
         finally {
            _releaseReply(in);
         }
      }
      else {
         // local call path
         org.omg.CORBA.portable.ServantObject so =
            _servant_preinvoke(operationName, java.lang.Object.class);
         if (so == null) 
            return invoke(operationName, stubStrategy, params);
         try {
            //params = Util.copyObjects(params, _orb());
            Object retval = 
               ((LocalIIOPInvoker)so.servant).invoke(operationName,
                                                     params,
                                                     null, /* tx */
                                                     null, /* identity */
                                                     null  /* credential */);
            return stubStrategy.convertLocalRetval(retval);
            //retval = stubStrategy.convertLocalRetval(retval);
            //return Util.copyObject(retval, _orb());
         }
         catch(Throwable e) {
            //Throwable ex = (Throwable)Util.copyObject(e, _orb());
            Throwable ex = e;
            if (stubStrategy.isDeclaredException(ex))
               throw ex;
            else
               throw Util.wrapException(ex);
         }
         finally {
            _servant_postinvoke(so);
         }
      }
   }
      
   /**
    * Sends a request message to the server, receives the reply from the 
    * server, and returns a <code>boolean</code> result to the caller.
    */
   public boolean invokeBoolean(String operationName,
                                StubStrategy stubStrategy, Object[] params)
         throws Throwable
   {
      return ((Boolean)invoke(operationName, 
                              stubStrategy, params)).booleanValue();
   }
   
   /**
    * Sends a request message to the server, receives the reply from the 
    * server, and returns a <code>byte</code> result to the caller.
    */
   public byte invokeByte(String operationName,
                          StubStrategy stubStrategy, Object[] params)
         throws Throwable 
   {
         return ((Number)invoke(operationName, 
                                stubStrategy, params)).byteValue();
      }
   
   /**
    * Sends a request message to the server, receives the reply from the 
    * server, and returns a <code>char</code> result to the caller.
    */
   public char invokeChar(String operationName,
                          StubStrategy stubStrategy, Object[] params)
         throws Throwable 
   {
      return ((Character)invoke(operationName, 
                                stubStrategy, params)).charValue();
   }
   
   /**
    * Sends a request message to the server, receives the reply from the 
    * server, and returns a <code>short</code> result to the caller.
    */
   public short invokeShort(String operationName,
                            StubStrategy stubStrategy, Object[] params)
         throws Throwable 
   {
      return ((Number)invoke(operationName, 
                             stubStrategy, params)).shortValue();
   }
   
   /**
    * Sends a request message to the server, receives the reply from the 
    * server, and returns an <code>int</code> result to the caller.
    */
   public int invokeInt(String operationName,
                        StubStrategy stubStrategy, Object[] params)
         throws Throwable 
   {
      return ((Number)invoke(operationName, stubStrategy, params)).intValue();
   }
   
   /**
    * Sends a request message to the server, receives the reply from the 
    * server, and returns a <code>long</code> result to the caller.
    */
   public long invokeLong(String operationName,
                          StubStrategy stubStrategy, Object[] params)
         throws Throwable 
   {
      return ((Number)invoke(operationName, stubStrategy, params)).longValue();
   }
   
   /**
    * Sends a request message to the server, receives the reply from the 
    * server, and returns a <code>float</code> result to the caller.
    */
   public float invokeFloat(String operationName,
                            StubStrategy stubStrategy, Object[] params)
         throws Throwable 
   {
      return ((Number)invoke(operationName, 
                             stubStrategy, params)).floatValue();
   }
   
   /**
    * Sends a request message to the server, receives the reply from the 
    * server, and returns a <code>double</code> result to the caller.
    */
   public double invokeDouble(String operationName,
                              StubStrategy stubStrategy, Object[] params)
         throws Throwable 
   {
      return ((Number)invoke(operationName, 
                             stubStrategy, params)).doubleValue();
   }
   
}
