// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: CodeMBean.java,v 1.14.2.4 2003/06/04 04:48:03 starksm Exp $
// ========================================================================

package org.mortbay.util.jmx;

import javax.management.InstanceNotFoundException;
import javax.management.MBeanException;
import org.mortbay.util.Code;


public class CodeMBean extends ModelMBeanImpl
{
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @exception MBeanException 
     * @exception InstanceNotFoundException 
     */
    public CodeMBean()
        throws MBeanException, InstanceNotFoundException
    {
        super(Code.instance());
    }

    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @exception MBeanException 
     * @exception InstanceNotFoundException 
     */
    public CodeMBean(Code code)
        throws MBeanException, InstanceNotFoundException
    {
        super(code);
    }

    /* ------------------------------------------------------------ */
    protected void defineManagedResource()
    {
        super.defineManagedResource();

        defineAttribute("debug");
        defineAttribute("suppressStack");
        defineAttribute("suppressWarnings");
        defineAttribute("verbose");
        defineAttribute("debugPatterns");
    }
}


