package org.jboss.tm;

import org.jboss.logging.Logger;
import javax.transaction.xa.XAException;



/**
 * XAExceptionFormatter.java
 *
 *
 * Created: Tue Jan 28 12:07:56 2003
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public interface XAExceptionFormatter {
   
   void formatXAException(XAException xae, Logger log);

}// XAExceptionFormatter
