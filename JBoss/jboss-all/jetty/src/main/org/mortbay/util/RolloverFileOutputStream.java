// ========================================================================
// Copyright (c) 1997 MortBay Consulting, Sydney
// $Id: RolloverFileOutputStream.java,v 1.14.2.8 2003/06/04 04:47:59 starksm Exp $
// ========================================================================

package org.mortbay.util;

import java.io.File;
import java.io.FileOutputStream;
import java.io.FilterOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.lang.ref.WeakReference;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Date;
import java.util.GregorianCalendar;
import java.util.ListIterator;
import java.util.StringTokenizer;

/* ------------------------------------------------------------ */
/** A File OutputStream that rolls overs.
 * If the passed filename contains the string "yyyy_mm_dd" on daily intervals.
 * @version $Id: RolloverFileOutputStream.java,v 1.14.2.8 2003/06/04 04:47:59 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
public class RolloverFileOutputStream extends FilterOutputStream
{
    private final static String YYYY_MM_DD="yyyy_mm_dd";
    private final static ArrayList __rollovers = new ArrayList();
    private static Rollover __rollover;

    private SimpleDateFormat _fileBackupFormat =
        new SimpleDateFormat(System.getProperty("ROLLOVERFILE_BACKUP_FORMAT","HHmmssSSS"));
    private SimpleDateFormat _fileDateFormat = 
        new SimpleDateFormat(System.getProperty("ROLLOVERFILE_DATE_FORMAT","yyyy_MM_dd"));

    private String _filename;
    private File _file;
    private boolean _append;
    private int _retainDays;
    private WeakReference _ref;
    
    /* ------------------------------------------------------------ */
    public RolloverFileOutputStream(String filename)
        throws IOException
    {
        this(filename,true,Integer.getInteger("ROLLOVERFILE_RETAIN_DAYS",31).intValue());
    }
    
    /* ------------------------------------------------------------ */
    public RolloverFileOutputStream(String filename, boolean append)
        throws IOException
    {
        this(filename,append,Integer.getInteger("ROLLOVERFILE_RETAIN_DAYS",31).intValue());
    }
    
    /* ------------------------------------------------------------ */
    public RolloverFileOutputStream(String filename,
                                    boolean append,
                                    int retainDays)
        throws IOException
    {
        super(null);
        
        if (filename!=null)
        {
            filename=filename.trim();
            if (filename.length()==0)
                filename=null;
        }
        if (filename==null)
            throw new IllegalArgumentException("Invalid filename");

        _filename=filename;
        _append=append;
        _retainDays=retainDays;
        _ref=new WeakReference(this);
        setFile();
        
        synchronized(__rollovers)
        {
            if (__rollover==null)
            {
                __rollover=new Rollover();
                __rollover.start();
            }
            __rollovers.add(_ref);
        }
    }

    /* ------------------------------------------------------------ */
    public String getFilename()
    {
        return _filename;
    }
    
    /* ------------------------------------------------------------ */
    public String getDatedFilename()
    {
        if (_file==null)
            return null;
        return _file.toString();
    }
    
    /* ------------------------------------------------------------ */
    public int getRetainDays()
    {
        return _retainDays;
    }

    /* ------------------------------------------------------------ */
    private synchronized void setFile()
        throws IOException
    {
        // Check directory
        File file = new File(_filename);
        _filename=file.getCanonicalPath();
        file=new File(_filename);
        File dir= new File(file.getParent());
        if (!dir.isDirectory() || !dir.canWrite())
            throw new IOException("Cannot write log directory "+dir);
            
        Date now=new Date();
        
        // Is this a rollover file?
        String filename=file.getName();
        int i=filename.toLowerCase().indexOf(YYYY_MM_DD);
        if (i>=0)
        {
            file=new File(dir,
                          filename.substring(0,i)+
                          _fileDateFormat.format(now)+
                          filename.substring(i+YYYY_MM_DD.length()));
        }
            
        if (file.exists()&&!file.canWrite())
            throw new IOException("Cannot write log file "+file);

        // Do we need to change the output stream?
        if (out==null || !file.equals(_file))
        {
            // Yep
            _file=file;
            if (!_append && file.exists())
                file.renameTo(new File(file.toString()+"."+_fileBackupFormat.format(now)));
            OutputStream oldOut=out;
            out=new FileOutputStream(file.toString(),_append);
            if (oldOut!=null)
                oldOut.close();
            Code.debug("Opened ",_file);
        }
    }

    /* ------------------------------------------------------------ */
    private void removeOldFiles()
    {
        if (_retainDays>0)
        {
            Calendar retainDate = Calendar.getInstance();
            retainDate.add(Calendar.DATE,-_retainDays);
            int borderYear = retainDate.get(java.util.Calendar.YEAR);
            int borderMonth = retainDate.get(java.util.Calendar.MONTH) + 1;
            int borderDay = retainDate.get(java.util.Calendar.DAY_OF_MONTH);

            File file= new File(_filename);
            File dir = new File(file.getParent());
            String fn=file.getName();
            int s=fn.toLowerCase().indexOf(YYYY_MM_DD);
            if (s<0)
                return;
            String prefix=fn.substring(0,s);
            String suffix=fn.substring(s+YYYY_MM_DD.length());

            String[] logList=dir.list();
            for (int i=0;i<logList.length;i++)
            {
                fn = logList[i];
                if(fn.startsWith(prefix)&&fn.indexOf(suffix,prefix.length())>=0)
                {        
                    try
                    {
                        StringTokenizer st = new StringTokenizer
                            (fn.substring(prefix.length()),
                             "_.");
                        int nYear = Integer.parseInt(st.nextToken());
                        int nMonth = Integer.parseInt(st.nextToken());
                        int nDay = Integer.parseInt(st.nextToken());
                        
                        if (nYear<borderYear ||
                            (nYear==borderYear && nMonth<borderMonth) ||
                            (nYear==borderYear &&
                             nMonth==borderMonth &&
                             nDay<=borderDay))
                        {
                            Log.event("Log age "+fn);
                            new File(dir,fn).delete();
                        }
                    }
                    catch(Exception e)
                    {
                        if (Code.debug())
                            e.printStackTrace();
                    }
                }
            }
        }
    }
    
    /* ------------------------------------------------------------ */
    /** 
     */
    public void close()
        throws IOException
    {
        synchronized(__rollovers)
        {
            __rollovers.remove(_ref);
            _ref=null;
            try{super.close();}
            finally
            {
                out=null;
                _file=null;
            }

            //  this will kill the thread when the last stream is removed.
            if ( __rollovers.size() == 0 ) {
                __rollover.timeToStop();
                __rollover.interrupt();
                __rollover = null;
            }
        }
    }
    
    
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    private class Rollover extends Thread
    {
        private boolean timeToStop = false;
        
        Rollover()
        {
            setName("Rollover");
            setDaemon(true);
        }
        
        synchronized void timeToStop()
        {
            timeToStop = true;
        }

        public void run()
        {
            while(!timeToStop)
            {
                try
                {
                    // Sleep until midnight
                    Calendar now = Calendar.getInstance();
                    GregorianCalendar midnight =
                        new GregorianCalendar(now.get(Calendar.YEAR),
                                              now.get(Calendar.MONTH),
                                              now.get(Calendar.DAY_OF_MONTH),
                                              23,0);
                    midnight.add(Calendar.HOUR,1);
                    long sleeptime=
                        midnight.getTime().getTime()-
                        now.getTime().getTime();
                    Code.debug("Rollover sleep until "+midnight.getTime());
                    Thread.sleep(sleeptime);
                }
                catch(InterruptedException e)
                {
                    if (!timeToStop)
                        e.printStackTrace();
                }
                    
                synchronized(__rollovers)
                {
                    ListIterator iter = __rollovers.listIterator();
                    while (iter.hasNext())
                    {
                        WeakReference ref =
                            (WeakReference)iter.next();
                        RolloverFileOutputStream rfos =
                            (RolloverFileOutputStream)ref.get();

                        if (rfos==null)
                            iter.remove();
                        else
                        {
                            try
                            {
                                rfos.setFile();
                                rfos.removeOldFiles();
                            }
                            catch(IOException e)
                            {
                                if (!timeToStop)
                                    e.printStackTrace();
                            }
                        }
                    }
                }
            }
        }
    }
}





