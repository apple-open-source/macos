// ========================================================================
// Copyright (c) 1997 MortBay Consulting, Sydney
// $Id: Frame.java,v 1.15.2.7 2003/07/11 00:55:02 jules_gosnell Exp $
// ========================================================================

package org.mortbay.util;

import java.io.PrintWriter;


/*-----------------------------------------------------------------------*/
/** Access the current execution frame.
 */
public class Frame
{
    /*-------------------------------------------------------------------*/
    /** Shared static instances, reduces object creation at expense
     * of lock contention in multi threaded debugging */
    private static Throwable __throwable = new Throwable();
    private static StringBuffer __stringBuffer = new StringBuffer();
    private static StringBufferWriter __stringBufferWriter = new StringBufferWriter(__stringBuffer);
    private static PrintWriter __printWriter = new PrintWriter(__stringBufferWriter,false);
    private static final String __lineSeparator = System.getProperty("line.separator");
    private static final int __lineSeparatorLen = __lineSeparator.length();
    
    /*-------------------------------------------------------------------*/
    /** The full stack of where the Frame was created. */
    private String _stack;
    /** The Method (including the "(file.java:99)") the Frame was created in */
    private String _method= "unknownMethod";
    /** The stack depth where the Frame was created (main is 1) */
    private int _depth=0;
    /** Name of the Thread the Frame was created in */
    private String _thread= "unknownThread";
    /** The file and linenumber of where the Frame was created. */
    private String _file= "UnknownFile";

    private String _where;
    private int _lineStart=0;
    private int _lineEnd;



    /*-------------------------------------------------------------------*/
    /** Construct a frame.
     */
    public Frame()
    {
        // Dump the stack
        synchronized(__printWriter)
        {
            __stringBuffer.setLength(0);
            __throwable.fillInStackTrace();
            __throwable.printStackTrace(__printWriter);
            __printWriter.flush();
            _stack = __stringBuffer.toString();
        }
        internalInit(0, false);
    }
    
    /*-------------------------------------------------------------------*/
    /** Construct a frame.
     * @param ignoreFrames number of levels of stack to ignore
     */
    public Frame(int ignoreFrames)
    {
        // Dump the stack
        synchronized(__printWriter)
        {
            __stringBuffer.setLength(0);
            __throwable.fillInStackTrace();
            __throwable.printStackTrace(__printWriter);
            __printWriter.flush();
            _stack = __stringBuffer.toString();
        }
        internalInit(ignoreFrames, false);
    }
    
    /* ------------------------------------------------------------ */
    /** package private Constructor. 
     * @param ignoreFrames Number of frames to ignore
     * @param partial Partial construction if true
     */
    Frame(int ignoreFrames, boolean partial)
    {
        synchronized(__printWriter)
        {
            __stringBuffer.setLength(0);
            __throwable.fillInStackTrace();
            __throwable.printStackTrace(__printWriter);
            __printWriter.flush();
            _stack = __stringBuffer.toString();
        }
        // Dump the stack
        internalInit(ignoreFrames, partial);
    }
    
    /* ------------------------------------------------------------ */
    /** Internal only Constructor. */
    private Frame(String stack, int ignoreFrames, boolean partial)
    {
        _stack = stack;
        internalInit(ignoreFrames, partial);
    }
    
    /* ------------------------------------------------------------ */
    void internalInit(int ignoreFrames, boolean partial)
    {
        // Extract stack components, after we look for the Frame constructor
 	// itself and pull that off the stack!
        
        _lineStart = 0;
 	_lineStart = _stack.indexOf("Frame.<init>(",_lineStart);
        if (_lineStart==-1)
        {
            // JIT has inlined Frame constructor
            _lineStart =
                _stack.indexOf(__lineSeparator)+__lineSeparatorLen;
        }
        
        
        _lineStart = _stack.indexOf(__lineSeparator,_lineStart)+
 	    __lineSeparatorLen;
        for (int i = 0; _lineStart > 0 && i < ignoreFrames; i++)
        {
            _lineStart = _stack.indexOf(__lineSeparator,_lineStart)+
                         __lineSeparatorLen;
        }
        _lineEnd = _stack.indexOf(__lineSeparator,_lineStart);
        
        if (_lineEnd < _lineStart || _lineStart < 0){
            _where = null;
            _stack = null;
        }
        else
        {
            _where = _stack.substring(_lineStart,_lineEnd);
            if (!partial) complete();
        }
    }
    
    /* ------------------------------------------------------------ */
    /** Complete partial constructor.
     */
    void complete()
    {
        // trim stack
        if (_stack != null) 
            _stack = _stack.substring(_lineStart);
        else
        {
            // Handle nulls
            if (_method==null)
                _method= "unknownMethod";
            if (_file==null)
                _file= "UnknownFile";
            return;
        }

        // calculate stack depth
        int i=0-__lineSeparatorLen;
        while ((i=_stack.indexOf(__lineSeparator,i+__lineSeparatorLen))>0)
                _depth++;
        
        // extract details
        if (_where!=null)
        {
            int lb = _where.indexOf('(');
            int rb = _where.indexOf(')');

            if (lb>=0 && rb >=0 && lb<rb) 
            {
                _file = _where.substring(lb+1,rb).trim();
                if (_file.indexOf(':') < 0)
                    _file = null;
            }
              
            int at = _where.indexOf("at ");
            if (at >=0 && (at+3)<_where.length())
                _method = _where.substring(at+3);
            if (at < 0 && rb > 0) 
            {
                _method = _where.trim();
                _method = _method.substring(_method.indexOf(' ') + 1);
            }
        }
        
        // Get Thread name
        _thread = Thread.currentThread().getName();

        // Handle nulls
        if (_method==null)
            _method= "unknownMethod";
        if (_file==null)
            _file= "UnknownFile";
    }
    
    
    /*-------------------------------------------------------------------*/
    public String getStack()
    {
        return _stack;
    }
    
    /*-------------------------------------------------------------------*/
    public String getMethod()
    {
        return _method;
    }
    
    /*-------------------------------------------------------------------*/
    public int getDepth()
    {
        return _depth;
    }
    
    /*-------------------------------------------------------------------*/
    public String getThread()
    {
        return _thread;
    }
    
    /*-------------------------------------------------------------------*/
    public String getFile()
    {
        return _file;
    }
    
    /*-------------------------------------------------------------------*/
    public String getWhere()
    {
        return _where;
    }
    
    /*-------------------------------------------------------------------*/
    public String toString()
    {
        return "["+_thread + "]" + _method;
    }
    
    /* ------------------------------------------------------------ */
    /** Get a Frame representing the function one level up in this frame.
     * @return parent frame or null if none
     */
    public Frame getParent()
    {
        Frame f = new Frame("Frame.<init>("+__lineSeparator+_stack, 1, false);
        if (f._where == null) return null;
        f._thread = _thread;
        return f;
    }    
}




