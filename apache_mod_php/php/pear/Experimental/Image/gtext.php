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
// | Authors: Ulf Wendel <ulf.wendel@phpdoc.de>                           |
// +----------------------------------------------------------------------+

require_once "Experimental/Image/color_helper.php";

/**
* Creates graphical texts. 
*
* FIXME - Usage example missing - FIXME
* 
* @author   Ulf Wendel <ulf.wendel@phpdoc.de>
* @version  $Id: gtext.php,v 1.1.1.1 2001/07/19 00:20:48 zarzycki Exp $
*/
class gtext extends ColorHelper {

    /**
    * Characters with descenders. 
    *
    * ImageTTFBox() gives you the correct width of a text but not the correct height.
    * It does not return the body size but the size from the baseline to the ascender.
    * For correct vertical alignment of texts that contain characters which have
    * descenders (font dependend) you must perform an extra test to get the correct
    * height. 
    *
    * The string is used as a regular expression. Make sure, that you escaped all
    * special characters.
    * 
    * @var      string  All characters with descenders in the font you use. 
    *                   Default should work with Arial.
    * @access   public
    */
    var $descenders = "fgjpqßyGJYµ|´,;{}\[\]§_@€ƒ„¢µ¶çýÿþ¿Ç";
    
    
    /**
    * String used to measure the size of descenders.
    *
    * buildImage() uses a dirty hack to measure the size of descenders. 
    * It generates an image with the descender sign and scans the image... 
    *
    * @var  string  String used to draw a test image to find the size of
    *               descenders.
    */
    var $descender_string = "gG";
    
    
    /**
    * Graphics cache object used to create the images
    * 
    * FIXME - description missing
    * 
    * @var  object  Cache_Graphics
    */
    var $cache;

    
    /**
    * Properties of the gtext image.
    * 
    *
    * @var  array
    */
    var $properties = array(
                            "fontsize"      => 20,
                            "fgcolor"       => "black",
                            "fontdir"       => "",
                            "font"          => "arial",
                            
                            "transparent"   => "white",
                            "bgcolor"       => "white",
                            
                            "padding"       => 4
                        );

    /**
    * Unused list of allowed properties
    *
    * FIXME - It's does not have any meaning - just to give you an idea on the features.
    *
    * @var  array
    */                        
    var $allowed = array(
                            "fontsize"      => "integer",
                            "fgcolor"       => "mixed",
                            "fontdir"       => "strint",

                            "bold"          => "boolean",
                            "italic"        => "boolean",
                            
                            "transparent"   => "mixed",
                            
                            "padding"       => "integer",
                            
                            "bgcolor"       => "mixed",
                            
                            "background"    => "string",
                            "bgstrech"      => "boolean",
                            "bgcenter"      => "boolean",
                            
                            "bevel"         => "integer",
                            "bevelcolor"    => "mixed",
                            
                            "statusbar"     => "string",
                            
                            "name"          => "string",
                            "href"          => "string"
                        );                        
       
       
    /**
    * Creates an instance of the graphics_cache class. 
    * 
    * @see  $cache
    */                        
    function gtext() {    
    
        $this->cache = new Cache_Graphics();
            
    } // end constructor
    
    
    /**
    * Creates a graphical text and returns the image stream.
    * 
    * @param    string  Text of the image
    * @param    mixed   List of properties that describe the image.
    *                   FIXME description missing
    * @param    string  Image format: gif, jpg, png, wbmp - make sure 
    *                   that your gd library features the requested format
    * @return   string  Image stream
    * @see      getImageTag()
    * @access   public
    */
    function getImage($text, $properties = "", $format = "png") {
        
        $properties = array_merge($this->properties, $properties);
        $id = $this->cache->generateID($text, $properties);
        
        if ($image = $cache->getImage($id, $format))
            return $image;

        $img = $this->buildImage($id, $text, $properties);
        
        return $this->cache->cacheImage($id, $img, $format);            
    } // end func getImage
    
    /**
    * Creates a graphical text and returns it as a <img>-Tag, optionally as hyperlink.
    *
    * @brother  getImage()
    */
    function getImageTag($text, $properties = "", $format = "png") {

        $properties = array_merge($this->properties, $properties);
        $id = $this->cache->generateID(array($text, $properties));

        $link = $this->cache->getImageLink($id, $format);
        if (count($link))
            return $this->createImageTag($id, $text, $properties, $link);
        
        $img = $this->buildImage($id, $text, $properties);
        $link = $this->cache->cacheImageLink($id, $img, $format);
        
        return $this->createImageTag($id, $text, $properties, $link);
    } // end func getImageTag
       
       
    /**
    * Creates a grahical text and return it as a <img>-Tag, optionally as hyperlink.
    * 
    * Private function that does all the work. getImageTag() is only the 
    * public wrapper for this and some other functions. 
    * 
    * @param    string  Image-ID used with the cache
    * @param    string  Image text
    * @param    array   Array that describes the properties of the image
    * @param    array   Array with the URL and the total file path to the image
    * @return   string  HTML tag.     
    */ 
    function createImageTag($id, $text, &$properties, $link) {
        
        $size = @getImageSize($link[0]);
        
        $name = (isset($properties["name"])) ? $properties["name"] : "";
        $js_over = "";
        $js_out = "";
        
        if (isset($properties["statusbar"])) {
            $name = $id;
            $js_over .= sprintf("window.status = '%s';", str_replace('"', "'", $properties["statusbar"]));
            $js_out  .= "window.status = '';";
        }
        
        $js = "";
        if ($js_over)
            $js .= sprintf(' onMouseOver="%s";', $js_over);
        if ($js_out)
            $js .= sprintf(' onMouseOut="%s";', $js_out);
        
        $html = sprintf('<img src="%s" %s alt="%s"%s%s>',
                            $link[1],
                            $size[3],
                            (isset($properties["alt"])) ? $properties["alt"] : $text,
                            ($name) ? ' name="' . $name . '"' : "",
                            ($js) ? $js : ""
                        );
        
        if (isset($properties["url"])) {
            
            $html = sprintf('<a href="%s"%s>%s</a>',
                                $properties["url"],
                                (isset($properties["target"])) ? ' target="' . $properties["target"] . '"' : ""
                            );
        }
        
        return $html;
    } // end func createImageTag

    /**
    * Image creation function. The magic happens here.
    * 
    * @param    string      Image-ID
    * @param    string      Image text
    * @param    array       Array that describes the properties of the image
    * @return   resource    Image handler of the created image. 
    */    
    function buildImage($id, $text, &$properties) {
        static $descender_cache = array();
        
        $font_dir = (isset($properties["fontdir"])) ? $properties["fontdir"] : $this->font_dir;
        
        // WARNING: this is a so called ulfismus - I always add a slash to the given path
        if ($font_dir && "/" != substr($font_dir, -1))
            $font_dir .= "/";
            
        
        $font = $font_dir . $properties["font"];
        
        // KLUDGE: This is not a proper test but better than nothing and 
        // windows seems to follow this naming convention widely.
        if (isset($properties["italic"])) {
        
            if (isset($properties["bold"]) && file_exists($font . "bi.ttf"))
                $font .= "bi";
            else if (file_exists($font . "i.tff"))
                $font .= "i";
            
        } else {
            
            if (isset($properties["bold"])) {
                if (file_exists($font . "bd.ttf"))
                    $font .= "bd";
                else if (file_exists($font . "b.ttf"))
                    $font .= "b";                    
            }
            
        }
        
        // TODO: Make gtext work with Postscript fonts.
        $font .= ".ttf";
        
        // size of the bounding text box
        $textsize = ImageTTFBBox($properties["fontsize"], 0, $font , $text);
        $font_width = abs($textsize[2] - $textsize[0]);
        $font_height = abs($textsize[3] - $textsize[5]);
        
        // TRICKY but WARNING: check for descenders, see below
        if (preg_match("/[" . $this->descenders . "]+/", $text)) {

            // Ok, this is really dirty code. Sit down before you go over it.
            // To find the size of the desecenders of the current font a new 
            // image is created that's filled from left to right with the 
            // configurable "descender_string". After that the image gets scanned
            // to find the size of the string... The scan starts at the lower left
            // corner and hopefully the script will soon find a pixel with the 
            // foreground color because the time it takes to scan it image is 
            // with * height...
            if (isset($descender_cache[$font][$properties["fontsize"]])) {
                
                $descender_diff = $descender_cache[$font][$properties["fontsize"]]["diff"];
                $font_height = $descender_cache[$font][$properties["fontsize"]]["total"];
                
            } else {
            
                // descender size
                $size = ImageTTFBBox($properties["fontsize"], 0, $font, $this->descender_string);
                
                $font_height = abs($size[3] - $size[5]);
                $descender_width = abs($size[2] - $size[0]);
                $descender_height = 2 * $font_height;
                

                                // temporary image to draw on
                $tmp_img = @ImageCreate($descender_width, $descender_height);
                $bg_color = $this->allocateColor($tmp_img, "white");
                $fg_color = $this->allocateColor($tmp_img, "black");
                
                for ($x = -$descender_width; $x < $descender_width; $x++)
                    ImageTTFText($tmp_img, $properties["fontsize"], 0, $x, round($descender_height) / 1.5, $fg_color, $font, $this->descender_string);
                
                for ($x = -$descender_width; $x < $descender_width; $x++)
                    ImageTTFText($tmp_img, $properties["fontsize"], 0, $x, round($descender_height) / 1.5, $bg_color, $font, "I");   
                    
                // scanning the height of a descender
                // search the lower right edge
                for ($yb = $descender_height; $yb >= 0; $yb--)
                    if (ImageColorAt($tmp_img, 2, $yb) == $fg_color)
                            break;

                for ($yt = $yb - 1; $yt >= 0; $yt--)
                    if (ImageColorAt($tmp_img, 2, $yt) != $fg_color)
                            break;

                $descender_diff = $yb - $yt;   
                $font_height += $descender_diff;                         
                
                // cache the result
                $descender_cache[$font][$properties["fontsize"]]["total"] = $font_height;
                $descender_cache[$font][$properties["fontsize"]]["diff"] = $descender_diff;
                
            }
 
                   
        }

        // image size = text size + special effects (borders...) size
        $image_width = $font_width + 2 * $properties["padding"];;
        $image_height = $font_height + 2 * $properties["padding"];;
        
        if (isset($properties["bevel"])) {
            $image_width += $properties["bevel"];
            $image_height += $properties["bevel"];
        }
        
        $img = @ImageCreate($image_width, $image_height);
        if (!$img)
             return new gerror("Can't allocate a new image.");

        // draw the background
        $bg_color = $this->allocateColor($img, $properties["bgcolor"]);
        ImageFilledRectangle($img, 0, 0, $image_width, $image_height, $bg_color);

        // background image? 
        if (isset($properties["background"]) && file_exists($properties["background"])) {
        
            if (!$size = @getimagesize($properties["background"]))
                return new gerror("Can't access the background image. Must be GIF, JPG or PNG format.");
            
            $format = array("", "GIF", "JPG", "PNG");
            $format = strtoupper($format[$size[1]]);
            if ("JPG" == $format)
                $format = "JPEG";

            // use a variable function to create an image from JPG, PNG; GIF, WMBP etc. 
            $func = "ImageCreateFrom$format";
            if (function_exists($func)) {
                
                $bg_img = $func($properties["background"]);
                if ($bg_img) {
                
                    // center the background image in the middle of the new image
                    if (isset($properties["bgcenter"])) {

                        // range checks otherwise the results may be unpredictable
                        if ($size[0] >= $image_width) {
                            $x = 0; 
                            $w = $image_width;
                        } else {
                            $x = ($image_width - $size[0]) / 2;
                            $w = $size[0];
                        }
                        
                        if ($size[1] >= $image_height) {
                            $y = 0;
                            $h = $image_height;
                        } else {
                            $y = ($image_height - $size[1]) / 2;
                            $h = $size[1];
                        }
                        
                        ImageCopy($img, $bg_img, $x, $y, 0, 0, $w, $h);
                        
                    } else if (isset($properties["bgstretch"])) {
                    
                        // strech the background image to the size of the new image
                        ImageCopyResized($img, $bg_img, 0, 0, 0, 0, $image_width, $image_height, $size[0], $size[1]);
                        
                    } else {
                        
                        for ($x = 0; $x <= $image_width; $x += $size[0]) {
                        
                            for ($y = 0; $y <= $image_height; $y += $size[1]) 
                                ImageCopy($img, $bg_img, $x, $y, 0, 0, $size[0], $size[1]);
                                
                            if ($y > $image_height)
                               ImageCopy($img, $bg_img, $x, $y - $size[0], 0, 0, $size[0], $y - $image_height);
                        }
                        
                        if ($x > $image_width) {
                        
                            $dx = $x - $image_width;
                            $x -= $size[0];
                            
                            for ($y = 0; $y < $image_height; $y += $size[1]) 
                                ImageCopy($img, $bg_img, $x, $y, 0, 0, $dx, $size[1]);
                                
                            if ($y > $image_height)
                               ImageCopy($img, $bg_img, $x, $y - $size[0], 0, 0, $dx, $y - $image_height);

                        }
                     
                    }
                    
                    ImageDestroy($bg_img);
                    
                } else {
                    
                    return new gerror("Can't create background image");
                    
                }

            }
                

        }
        
        // draw the bevel
        if (isset($properties["bevel"])) {
        
            $bevel_color = (isset($properties["bevelcolor"])) ? $properties["bevelcolor"] : $properties["fgcolor"];
            $bevel_color = $this->allocateColor($img, $bevel_color);
            
            for ($i = 1; $i <= $properties["bevel"]; $i++) {
                ImageLine($img, $i - 1, $image_height - $i, $image_width, $image_height - $i, $bevel_color);
                ImageLine($img, $image_width - $i, $i, $image_width - $i, $image_height, $bevel_color);
            }
        }
        
        // the text itself
        $fg_color = $this->allocateColor($img, $properties["fgcolor"]);
        if (isset($properties["padding"]) && $properties["padding"] > 0)
            $padding = round($properties["padding"] / 2);
        else 
            $padding = 0;

        $middle = ($image_height / 2) + ($font_height / 2) - $descender_diff;
        ImageTTFText($img, $properties["fontsize"], 0, $padding, $middle, $fg_color, $font, $text);
        
        // set the transparent color
        if (isset($properties["transparent"])) {
            $trans = $this->allocateColor($img, $properties["transparent"]);
            ImageColorTransparent($img, $trans);
        }
        
        return $img;
    } // end func buildImage
    
} // end class gtext
?>