<?php
// +----------------------------------------------------------------------+
// | PHP version 4.0                                                      |
// +----------------------------------------------------------------------+
// | Copyright (c) 1997, 1998, 1999, 2000, 2001 The PHP Group             |
// +----------------------------------------------------------------------+
// | This source file is subject to version 2.0 of the PHP license,       |
// | that is bundled with this package in the file LICENSE, and is        |
// | available at through the world-wide-web at                           |
// | http://www.php.net/license/2_02.txt.                                 |
// | If you did not receive a copy of the PHP license and are unable to   |
// | obtain it through the world-wide-web, please send a note to          |
// | license@php.net so we can mail you a copy immediately.               |
// +----------------------------------------------------------------------+
// | Authors: Christian Stocker <chregu@nomad.ch>                         |
// +----------------------------------------------------------------------+
//
// $Id: fo2pdf.php,v 1.1.1.1 2001/07/19 00:20:48 zarzycki Exp $


/**
* fo to pdf converter.
*
* with fo (formating objects) it's quite easy to convert xml-documents into
*  pdf-docs.
*
* An introduction into formating objects can be found at
*  http://www.w3.org/TR/xsl/slice6.html#fo-section
*  http://www.ibiblio.org/xml/books/bible/updates/15.html
* A tutorial is here:
*  http://www.xml.com/pub/a/2001/01/17/xsl-fo/
*  http://www.xml.com/pub/a/2001/01/24/xsl-fo/
* A html_to_fo.xsl can also be found there
*  http://www.xml.com/2001/01/24/xsl-fo/fop_article.tgz
*  but it didn't work for my simple xhtml files..
*
* The way to use this class is, produce a fo-file from a xml-file with a
* xsl-stylesheet, then feed this class with this fo-file and you get a pdf
* back (either directly to the browser for really dynamic-pdf production or
* as a file on your filesystem)
*
* It is recommended to use the Cache-Classes from PEAR, if you want dynamic
* pdf production, since the process of making the pdfs takes some time. For
* an example of how to  use Cache and fo2pdf see below.
*
* Requirements:
*
*  You need Fop from the xml-apache project (http://xml.apache.org/fop) and
*   Java (1.1.x or later, i tested it with 1.2.2 from sun on linux, see the
*   Fop-Docs for details).
*  Furthermore you have to compile your php with --with-java and to adjust
*   your php.ini file. It can be a rather painful task to get java and php
*   to work together.
*   See http://www.phpbuilder.com/columns/marknold20001221.php3 or
*   http://www.linuxwebdevnews.com/articles/php-java-xslt.php?pid=347
*   for more details about java and php or ask me, if you're stuck
*   (especially with linux. windows is not my area..)
*
* Todo:
*  - Errordetection
*  - Use embedding instead of org.apache.fop.apps.CommandLine-
*     this way, we maybe do not have to write temp-files and
*     we can render other stuff than only pdf (txt,pcl,ps,...)
*     see http://xml.apache.org/fop/embedding.html for details
*
* Usage:
*    require_once("XML/fo2pdf.php");
*    //make a pdf from simple.fo and save the pdf in a tmp-folder
*    $fop = new xml_fo2pdf("simple.fo");
*    //print pdf to the outputbuffer,
*    // including correct Header ("Content-type: application/pdf")
*    $fop->printPDF();
*    //delete the temporary pdf file
*    $fop->deletePDF();
*
*   With Cache:
*    require_once("XML/fo2pdf.php");
*    require_once("Cache/Output.php");
*    $container = "file";
*    $options = array("cache_dir"=>"/tmp/");
*    $cache = new Cache_Output("$container",$options);
*    $cache_handle = $cache->generateID($REQUEST_URI);
*    if ($content = $cache->start($cache_handle)) {
*      Header("Content-type: application/pdf");
*      print $content;
*      die();
*    }
*    $fop = new xml_fo2pdf("simple.fo");
*    $fop->printPDF();
*    $fop->deletePDF();
*    print $cache->end("+30");
*
* @author   Christian Stocker <chregu@nomad.ch>
* @version  $Id: fo2pdf.php,v 1.1.1.1 2001/07/19 00:20:48 zarzycki Exp $
* @package  XML
*/

class XML_fo2pdf {

    /**
    * fo-file used in this class
    *
    * @var  string
    */
    var $fo = "";

    /**
    * pdf-file used in this class
    *
    * @var  string
    */
    var $pdf = "";

    /**
    * Where the temporary fo and pdf files should be stored
    *
    * @var  string
    */
    var $tmpdir = "/tmp";

    /**
    * A prefix for the temporary files
    *
    * @var  string
    */
    var $tmppdfprefix = "pdffo";


    /**
    * constructor
    * One can pass an input fo-file already here (the other possibility
    * is with the run or runFromString method).
    *  and if the pdf should be stored permanently, a filename/path for
    *  that can also be passed here.
    *
    * @param    string  file input fo-file
    * @param    string  file output pdf-file
    * @see run(), runFromString(), runFromFile()
    */
    function xml_fo2pdf($fo = "", $pdf = "")
    {
        if ($fo) {
            $this->run($fo, $pdf);
        }
    }

    /**
    * Calls the Main Fop-Java-Programm
    *
    * One has to pass an input fo-file
    *  and if the pdf should be stored permanently, a filename/path for
    *  the pdf.
    *  if the pdf is not passed or empty/false, a temporary pdf-file
    *   will be created
    *
    * @param    string      file input fo-file
    * @param    string      file output pdf-file
    * @param    boolean     if the fo should be deleted after execution
    * @see runFromString()
    */
    function run($fo, $pdf = "", $DelFo = False)
    {
        if (!$pdf)
            $pdf = tempnam($this->tmpdir, $this->tmppdfprefix);

        $this->pdf = $pdf;
        $this->fo = $fo;
        $options = array($this->fo, $this->pdf);
        $java = new Java("org.apache.fop.apps.CommandLine", $options);
        $java->run();
        if ($DelFo) {
            $this->deleteFo($fo);
        }
    }

    /**
    * If the fo is a string, not a file, use this.
    *
    * If you generate the fo dynamically (for example with a
    *  xsl-stylesheet), you can use this method
    *
    * The Fop-Java program needs a file as an input, so a
    *  temporary fo-file is created here (and will be deleted
    *  in the run() function.)
    *
    * @param    string  fo input fo-string
    * @param    string  file output pdf-file
    * @see run()
    */
    function runFromString($fostring, $pdf = "")
    {
        $fo = tempnam($this->tmpdir, $this->tmppdfprefix);
        $fp = fopen($fo, "w+");
        fwrite($fp, $fostring);
        fclose($fp);
        $this->run($fo, $pdf, True);
    }
    /**
    * A wrapper to run for better readabilty
    *
    * This method just calls run....
    *
    * @param    string  fo input fo-string
    * @param    string  file output pdf-file
    * @see run()
    */
    function runFromFile($fo, $pdf = "")
    {
        return $this->run($fo, $pdf);
    }

    /**
    * Deletes the created pdf
    *
    * If you dynamically create pdfs and you store them
    *  for example in a Cache, you don't need it afterwards.
    * If no pdf is given, the one generated in run() is deleted
    *
    * @param    string  file output pdf-file
    */
    function deletePDF($pdf = "")
    {
        if (!$pdf)
            $pdf = $this->pdf;
        unlink ($pdf);
    }

    /**
    * Deletes the created fo
    *
    * If you dynamically create fos, you don't need it afterwards.
    * If no fo-file is given, the one generated in run() is deleted
    *
    * @param    string  file input fo-file
    */
    function deleteFo($fo = "")
    {
        if (!$fo)
            $fo = $this->fo;

        unlink ($fo);
    }

    /**
    * Prints the content header and the generated pdf to the output
    *
    * If you want to dynamically generate pdfs and return them directly
    *  to the browser, use this.
    * If no pdf-file is given, the generated from run() is taken.
    *
    * @param    string  file output pdf-file
    * @see returnPDF()
    */
    function  printPDF($pdf = "")
    {
        $pdf = $this->returnPDF($pdf);
        Header("Content-type: application/pdf\nContent-Length: " . strlen($pdf));
        print $pdf;
    }

    /**
    * Returns the pdf
    *
    * If no pdf-file is given, the generated from run() is taken.
    *
    * @param    string file output pdf-file
    * @return   string pdf
    * @see run()
    */
    function returnPDF($pdf = "")
        {
       if (!$pdf)
           $pdf = $this->pdf;

       $fd = fopen($pdf, "r");
       $content = fread( $fd, filesize($pdf) );
       fclose($fd);
       return $content;
    }

}
?>
