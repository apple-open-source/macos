<?
/* vim: set expandtab tabstop=4 shiftwidth=4: */
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
// | Authors: Adam Daniel <adaniel1@eesus.jnj.com>                        |
// +----------------------------------------------------------------------+
//
// $Id: Page.php,v 1.1.1.1 2001/07/19 00:20:49 zarzycki Exp $

require_once "HTML/Common.php";

/**
 * Base class for HTML pages
 *
 * This class handles the details for creating a properly constructed HTML page.
 * Page caching, stylesheets, client side script, and Meta tags can be 
 * managed using this class.
 * @author       Adam Daniel <adaniel1@eesus.jnj.com>
 * @version      1.0
 * @since        PHP 4.0.3pl1
 */
class HTML_Page extends HTML_Common {

    /**
     * Controls caching of the page
     * @var  bool
     * @access   private
     */
    var $_cache = False;

    /**
     * HTML page title
     * @var  string
     * @access   private
     */
    var $_title = "";

    /**
     * Array of meta tags
     * @var  array
     * @access   private
     */
    var $_metaTags = array("GENERATOR"=>"PEAR HTML_Page");

    /**
     * Array of linked style sheets
     * @var  array
     * @access   private
     */
    var $_styleSheets = array();

    /**
     * Array of linked scripts
     * @var  array
     * @access   private
     */
    var $_scripts = array();

    /**
     * Contents of HTML &lt;BODY&gt; tag 
     * @var  mixed
     * @access   public
     */
    var $body = "";

    /**
     * Returns the HTML page 
     * @access   public
     * @return string of HTML
     */
    function toHtml() 
    {
        $strName = "";
        $strContent = "";
        $intCounter = 0;
        if ($this->_comment) {
            $strHtml = "<!-- $this->_comment -->\n";
        }
        $strHtml .= "<HTML>\n";
        $strHtml .= "<HEAD>\n";
        $strHtml .= "<TITLE>$this->_title</TITLE>\n";
        for(reset($this->_metaTags); $strName = key($this->_metaTags); next($this->_metaTags)) {
            $strContent = pos($this->_metaTags);
            $strHtml .= "<META name=\"$strName\" content=\"$strContent\">\n";
        }
        for($intCounter=0; $intCounter<count($this->_styleSheets); $intCounter++) {
            $strStyleSheet = $this->_styleSheets[$intCounter];
            $strHtml .= "<LINK rel=\"stylesheet\" href=\"$strStyleSheet\" type=\"text/css\">\n"; 
        }
        for($intCounter=0; $intCounter<count($this->_scripts); $intCounter++) {
            $strType = $this->_scripts[$intCounter]["type"];
            $strSrc = $this->_scripts[$intCounter]["src"];
            $strHtml .= "<SCRIPT language=\"$strType\" src=\"$strSrc\"></SCRIPT>\n"; 
        }
        $strHtml .= "</HEAD>\n";
        $strAttr = $this->_getAttrString($this->_attributes);
        $strHtml .= "<BODY $strAttr>\n";
        if (is_object($this->body)) {
            if (method_exists($this->body, "toHtml")) {
                $strHtml .= $this->body->toHtml();
            } elseif (method_exists($contents, "toString")) {
                $strHtml .= $contents->toString();
            }
        } else {
            $strHtml .= $this->body;
        }
        $strHtml .= "</BODY>\n";
        $strHtml .= "</HTML>\n";
        return $strHtml;
    } // end func toHtml
    
    /**
     * Displays the HTML page to screen
     * @access    public
     */
    function display()
    {
        if(! $this->_cache) {
            header("Expires: Tue, 1 Jan 1980 12:00:00 GMT");
            header("Last-Modified: " . gmdate("D, d M Y H:i:s") . " GMT");
            header("Cache-Control: no-cache");
            header("Pragma: no-cache");
        }
        $strHtml = $this->toHtml();
        header("Content-Length: " . strlen($strHtml));
        print $strHtml;
    } // end func display

    /**
     * Adds a linked style sheet to the page
     * @param    string  $url    URL to the linked style sheet
     * @access   public
     */
    function addStyleSheet($url) 
    {
        $this->_styleSheets[] = $url;
    } // end func addStyleSheet

    /**
     * Adds a linked script to the page
     * @param    string  $url    URL to the linked style sheet
     * @param    string  $type   (optional) Type of script. Defaults to 'javascript'
     * @access   public
     */
    function addScript($url, $type="javascript") 
    {
        $this->_scripts[] = array("type"=>$type, "src"=>$url);
    } // end func addScript

    /**
     * Adds a meta tag to the page
     * @param    string  $name       META tag name
     * @param    string  $content   META tag contents
     * @access   public
     */
    function addMetaData($name, $content) 
    {
        $this->_metaTags[$name] = $content;
    } // end func addMetaData

    /**
     * Sets the caching of the page
     * @param    bool    $cache  Set to false to turn page caching off
     * @access   public
     */
    function setCache($cache)
    {
        $this->_cache = $cache;
    } // end func setCache

    /**
     * Sets the title of the page
     * @param    string    $title 
     * @access   public
     */
    function setTitle($title)
    {
        $this->_title = $title;
    } // end func setTitle

    /**
     * Return the title of the page
     * @access   public
     * @returns  string
     */
    function getTitle()
    {
        return $this->_title;
    } // end func getTitle

} // end class HTML_Page
?>
