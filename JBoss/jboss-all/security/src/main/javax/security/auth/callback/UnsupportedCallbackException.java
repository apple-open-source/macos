/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.security.auth.callback;

/** The JAAS 1.0 classes for use of the JAAS authentication classes with
 * JDK 1.3. Use JDK 1.4+ to use the JAAS authorization classes provided by
 * the version of JAAS bundled with JDK 1.4+.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public class UnsupportedCallbackException extends Exception
{
   private Callback callback;

   public UnsupportedCallbackException(Callback callback)
   {
      this(callback, null);
   }
   public UnsupportedCallbackException(Callback callback, String msg)
   {
      super(msg);
      this.callback = callback;
   }

   public Callback getCallback()
   {
      return callback;
   }

}
