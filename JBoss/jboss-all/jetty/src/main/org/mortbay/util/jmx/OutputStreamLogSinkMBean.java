// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: OutputStreamLogSinkMBean.java,v 1.14.2.4 2003/06/04 04:48:04 starksm Exp $
// ========================================================================

package org.mortbay.util.jmx;

import javax.management.MBeanException;
import org.mortbay.util.OutputStreamLogSink;

/* ------------------------------------------------------------ */
/** 
 *
 * @version $Revision: 1.14.2.4 $
 * @author Greg Wilkins (gregw)
 */
public class OutputStreamLogSinkMBean extends LogSinkMBean
{
    private boolean _formatOptions;

    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @exception MBeanException 
     */
    public OutputStreamLogSinkMBean()
        throws MBeanException
    {
        _formatOptions=true;
    }
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @exception MBeanException 
     */
    public OutputStreamLogSinkMBean(OutputStreamLogSink sink)
        throws MBeanException
    {
        super(sink);
        _formatOptions=true;
    }
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @param formatOptions 
     * @exception MBeanException 
     */
    public OutputStreamLogSinkMBean(boolean formatOptions)
        throws MBeanException
    {
        _formatOptions=formatOptions;
    }

    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @param formatOptions 
     * @exception MBeanException 
     */
    public OutputStreamLogSinkMBean(OutputStreamLogSink sink,boolean formatOptions)
        throws MBeanException
    {
        super(sink);
        _formatOptions=formatOptions;
    }
    
    /* ------------------------------------------------------------ */
    protected void defineManagedResource()
    {
        super.defineManagedResource();
        if (_formatOptions)
        {
            defineAttribute("logDateFormat");
            defineAttribute("logTimezone");
            defineAttribute("logTimeStamps");
            defineAttribute("logLabels");
            defineAttribute("logTags");
            defineAttribute("logStackSize");
            defineAttribute("logStackTrace");
            defineAttribute("logOneLine");
        }
        
        defineAttribute("append");
        defineAttribute("filename");
        defineAttribute("datedFilename");
        defineAttribute("retainDays");
        defineAttribute("flushOn");
    }
    
}


