<?php
//
// +----------------------------------------------------------------------+
// | PHP version 4.0                                                      |
// +----------------------------------------------------------------------+
// | Copyright (c) 1997-2001 The PHP Group                                |
// +----------------------------------------------------------------------+
// | This source file is subject to version 2.0 of the PHP license,       |
// | that is bundled with this package in the file LICENSE, and is        |
// | available at through the world-wide-web at                           |
// | http://www.php.net/license/2_02.txt.                                 |
// | If you did not receive a copy of the PHP license and are unable to   |
// | obtain it through the world-wide-web, please send a note to          |
// | license@php.net so we can mail you a copy immediately.               |
// +----------------------------------------------------------------------+
// | Authors: Ulf Wendel <ulf.wendel@phpdoc.de                            |
// +----------------------------------------------------------------------+
//
// $Id: gbutton.php,v 1.1.1.1 2001/07/19 00:20:48 zarzycki Exp $
// 

require_once "Experimental/Image/gtext.php";
/**
* Creates graphical buttons. 
* 
* @author   Ulf Wendel <ulf.wendel@phpdoc.de>
* @version  $Id: gbutton.php,v 1.1.1.1 2001/07/19 00:20:48 zarzycki Exp $
*/
class gbutton extends gtext {

    var $properties = array(
                            "fontsize"      => 20,
                            "fgcolor"       => "black",
                            "fontdir"       => "c:/www/apache/gtext/",
                            "font"          => "arial",
                            
                            "border"        => 1,
                            "bordercolor"   => "black",
                            
                            "padding"       => 2
                        );

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
                            
                            "border"        => "integer",
                            "bordercolor"   => "mixed",
                            
                            "3d"            => "integer",
                            "3dlightcolor"  => "mixed",
                            "3ddarkcolor"   => "mixed",
                             
                            "statusbar"     => "string",
                            
                            "name"          => "string",
                            "submit"        => "boolean",
                            "href"          => "string",
                            
                            "align"         => "string",
                            "valign"        => "string",
                            
                            "width"         => "integer",
                            "height"        => "integer"
                            
                        );                        
     
    function createImageTag($id, $text, &$properties, $link) {
        
        $size = getImageSize($link[0]);
        
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
        
        if (isset($properties["submit"])) {
            
            $html = sprintf('<input type="submit"%s%s>',
                                ($name) ? ' name="' . $name . '"' : "",
                                ($js) ? $js : ""
                            );
            
        } else {
        
            $html = sprintf('<img src="%s" %s alt="%s"%s%s>',
                                $link[1],
                                $size[3],
                                (isset($properties["alt"])) ? $properties["alt"] : $text,
                                ($name) ? ' name="' . $name . '"' : "",
                                ($js) ? $js : ""
                            );
        }
        
        if (isset($properties["href"])) {
            
            $html = sprintf('<a href="%s"%s>%s</a>',
                                $properties["url"],
                                (isset($properties["target"])) ? ' target="' . $properties["target"] . '"' : ""
                            );
        }
        
        return $html;
    } // end func createImageTag
    
    function &buildImage($id, $text, &$properties) {

        $font_dir = (isset($properties["fontdir"])) ? $properties["fontdir"] : $this->font_dir;
        if ($font_dir && "/" != substr($font_dir, -1))
            $font_dir .= "/";
            
        
        $font = $font_dir . $properties["font"];
        
        // this is not a proper test but better than nothing and 
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
        $font .= ".ttf";
        
        // size of the bounding text box
        $textsize = ImageTTFBBox($properties["fontsize"], 0, $font , $text);

        $tx = ($textsize[2] - $textsize[0]) + $properties["padding"];
        $ty = ($textsize[3] - $textsize[5]) + $properties["padding"];

        $border = 0;
        if (isset($properties["border"]))
            $border += (2 * $properties["border"]);
        
        if (isset($properties["3d"]))
            $border += (2 * $properties["3d"]);
        
        // border size
        $sx = $tx + $border;
        $sy = $ty + $border;
        
        // min width and height
        if (isset($properties["width"]) && $sx < $properties["width"]) 
            $sx = $properties["width"];
            
        if (isset($properties["height"]) && $sy < $properties["height"])
            $sy = $properties["height"];
            
        
        $img = @ImageCreate($sx, $sy);
        if (!$img)
             return new gerror("Can't create Image.");

        // draw the background
        $bg_color = $this->allocateColor($img, $properties["bgcolor"]);
        ImageFilledRectangle($img, 0, 0, $sx, $sy, $bg_color);

        if (isset($properties["border"])) {
        
            $border_color = (isset($properties["bordercolor"])) ? $properties["bordercolor"] : $properties["fgcolor"];
            $border_color = $this->allocateColor($img, $border_color);
            
            for ($i = 1; $i <= $properties["border"]; $i++)
                ImageRectangle($img, $i, $i, $sx - $i, $sy - $i, $border_color);
                
        }
               
        if (isset($properties["3d"])) {
            
            $d_light   = (isset($properties["3dlightcolor"])) ? $properties["3dlightcolor"] : $properties["bgcolor"];
            $d_light   = $this->allocateColor($img, $d_light);

            $d_dark    = (isset($properties["3ddarkcolor"])) ? $properties["3ddarkcolor"] : $properties["fgcolor"];
            $d_dark    = $this->allocateColor($img, $d_dark);
            
            for ($i = 1; $i <= $properties["3d"]; $i++) {
                ImageLine($img, $i - 1, $i - 1, $i - 1, $sy - $i, $d_light);
                ImageLine($img, $i - 1, $i - 1, $sx - $i , $i - 1, $d_light);
            
                ImageLine($img, $i - 1, $sy - $i, $sx, $sy - $i, $d_dark);
                ImageLine($img, $sx - $i, $i, $sx - $i, $sy, $d_dark);
            }
            
        }

        // preparing to render the text
        $fg_color = $this->allocateColor($img, $properties["fgcolor"]);
        $pad = $properties["padding"] / 2;
        $border /= 2;
        
        // checking the horizontal alignment
        if (isset($properties["align"])) {
           
            $align = strtolower($properties["align"]);
            if ("center" == $align) {
                $x = (($sx - $tx) / 2) + $pad;
            } else if ("right" == $align) {
                $x = $sx - $tx - $pad - $border ;
            } else {
                // default: left
                $x = $pad + $border;
            }
            
        } else {
            // default: left
            $x = $pad + $border;
        }
        
        // checking the vertical alignment
        if (isset($properties["valign"])) {
            
            $valign = strtolower($properties["valign"]);
            if ("top" == $valign) {
                $y = $sy - ($sy - $ty) + $border + $pad;
            } else if ("bottom" == $valign) {
                $y = $sy - $ty + $border - $pad;
            } else {
                // default: middle
                $y = $sy - (($sy - $ty) / 2) + $border - $pad;
            }
            
        } else {
            // default: middle
            $y = $sy - (($sy - $ty) / 2) + $border - $pad;
        }

        // rendering the label itself
        ImageTTFText($img, $properties["fontsize"], 0, $x, $y, $fg_color, $font, $text);

        // set the transparent color
        if (isset($properties["transparent"])) {
            $trans = $this->allocateColor($img, $properties["transparent"]);
            ImageColorTransparent($img, $trans);
        }
        
        return $img;
    } // end func buildImage
    
} // end class gbutton
?>