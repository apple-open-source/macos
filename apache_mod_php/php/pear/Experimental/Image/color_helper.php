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
// $Id: color_helper.php,v 1.1.1.1 2001/07/19 00:20:48 zarzycki Exp $
// 

/**
* Widget stuff: color translation, ...
* 
* Several widget functions to deal with images especially simple
* functions to convert userdefined colors into RGB colors.
*
* @author   Ulf Wendel <ulf.wendel@phpdoc.de>
* @version  $Id: color_helper.php,v 1.1.1.1 2001/07/19 00:20:48 zarzycki Exp $
*/
class ColorHelper {


    /**
    * Mapping from named colors to RGB values.
    * 
    * @var  array
    * @see  color2RGB()
    */ 
    var $colornames = array(    
                                "AliceBlue"     => array(240, 248, 255),
                                "AntiqueWhite"  => array(250, 235, 215),
                                "Aqua"          => array(0, 255, 255),
                                "Aquamarine"    => array(127, 255, 212),
                                "Azure"         => array(240, 255,  255),
                                "Beige"         => array(245, 245, 220),
                                "Bisque"        => array(255, 228, 196),
                                "Black"         => array(0, 0, 0),
                                "BlanchedAlmond"=> array(255, 235, 205),
                                "Blue"          => array(0, 0, 255),
                                "BlueViolet"    => array(138, 43, 226),
                                "Brown"         => array(165, 42, 42),
                                "BurlyWood"     => array(222, 184, 135),
                                "CadetBlue"     => array(95, 158, 160),
                                "Chartreuse"    => array(127, 255, 0),
                                "Chocolate"     => array(210, 105, 30),
                                "Coral"         => array(255, 127, 80),
                                "CornflowerBlue"=> array(100, 149,  237),
                                "Cornsilk"      => array(255, 248, 220),
                                "Crimson"       => array(220, 20, 60),
                                "Cyan"          => array(0, 255, 255),
                                "DarkBlue"      => array(0, 0, 13), 
                                "DarkCyan"      => array(0, 139, 139),
                                "DarkGoldenrod" => array(184, 134, 11),
                                "DarkGray"      => array(169, 169, 169),
                                "DarkGreen"     => array(0, 100, 0),
                                "DarkKhaki"     => array(189, 183, 107),
                                "DarkMagenta"   => array(139, 0, 139),
                                "DarkOliveGreen"=> array(85, 107, 47),
                                "DarkOrange"    => array(255, 140, 0),
                                "DarkOrchid"    => array(153, 50, 204),
                                "DarkRed"       => array(139, 0, 0),
                                "DarkSalmon"    => array(233, 150, 122),
                                "DarkSeaGreen"  => array(143, 188, 143),
                                "DarkSlateBlue" => array(72, 61, 139),
                                "DarkSlateGray" => array(47, 79, 79),
                                "DarkTurquoise" => array(0, 206, 209),
                                "DarkViolet"    => array(148, 0, 211),
                                "DeepPink"      => array(255, 20, 147),
                                "DeepSkyBlue"   => array(0, 191, 255),
                                "DimGray"       => array(105, 105, 105),
                                "DodgerBlue"    => array(30, 144, 255),
                                "FireBrick"     => array(178, 34, 34),
                                "FloralWhite"   => array(255, 250, 240),
                                "ForestGreen"   => array(34, 139, 34),
                                "Fuchsia"       => array(255, 0, 255),
                                "Gainsboro"     => array(220, 220, 220),
                                "GhostWhite"    => array(248, 248, 255),
                                "Gold"          => array(255, 215, 0),
                                "Goldenrod"     => array(218, 165, 32),
                                "Gray"          => array(128, 128, 128),
                                "Green"         => array(0, 128, 0),
                                "GreenYellow"   => array(173, 255, 47),
                                "Honeydew"      => array(240, 255, 240),
                                "HotPink"       => array(255, 105, 180),
                                "IndianRed"     => array(205, 92, 92),
                                "Indigo"        => array(75, 0, 130),
                                "Ivory"         => array(255, 255, 240),
                                "Khaki"         => array(240, 230, 140),
                                "Lavender"      => array(230, 230, 250),
                                "LavenderBlush" => array(255, 240, 245),
                                "LawnGreen"     => array(124, 252,  0),
                                "LemonChiffon"  => array(255, 250, 205),
                                "LightBlue"     => array(173, 216, 230),
                                "LightCoral"    => array(240, 128, 128),
                                "LightCyan"     => array(224, 255, 255),
                                "LightGoldenrodYellow" => array(250, 250, 210),
                                "LightGreen"    => array(144, 238, 144),
                                "LightGrey"     => array(211, 211, 211),
                                "LightPink"     => array(255, 182, 193),
                                "LightSalmon"   => array(255, 160, 122),
                                "LightSeaGreen" => array(32, 178, 170),
                                "LightSkyBlue"  => array(135, 206, 250),
                                "LightSlateGray"=> array(119, 136, 153),
                                "LightSteelBlue"=> array(176, 196, 222),
                                "LightYellow"   => array(255, 255, 224),
                                "Lime"          => array(0, 255, 0),
                                "LimeGreen"     => array(50, 205, 50),
                                "Linen"         => array(250, 240, 230),
                                "Magenta"       => array(255, 0, 255),
                                "Maroon"        => array(128, 0, 0),
                                "MediumAquamarine" => array(102, 205, 170),
                                "MediumBlue"    => array(0, 0, 205),
                                "MediumOrchid"  => array(186, 85, 211),
                                "MediumPurple"  => array(147, 112, 219),
                                "MediumSeaGreen"=> array(60, 179, 113),
                                "MediumSlateBlue"   => array(123, 104, 238),
                                "MediumSpringGreen" => array(0, 250, 154),
                                "MediumTurquoise"   => array(72, 209, 204),
                                "MediumVioletRed"   => array(199, 21, 133),
                                "MidnightBlue"  => array(25, 25, 112),
                                "MintCream"     => array(245, 255, 250),
                                "MistyRose"     => array(255, 228, 225),
                                "Moccasin"      => array(255, 228, 181),
                                "NavajoWhite"   => array(255, 222, 173),
                                "Navy"          => array(0, 0, 128),
                                "OldLace"       => array(253, 245, 230),
                                "Olive"         => array(128, 128, 0),
                                "OliveDrab"     => array(107, 142, 35),
                                "Orange"        => array(255, 165, 0),
                                "OrangeRed"     => array(255,69,0),
                                "Orchid"        => array(218,112,214),
                                "PaleGoldenrod" => array(238,232,170),
                                "PaleGreen" => array(152,251,152),
                                "PaleTurquoise" => array(175,238,238),
                                "PaleVioletRed" => array(219,112,147),
                                "PapayaWhip" => array(255,239,213),
                                "PeachPuff" => array(255,218,185),
                                "Peru" => array(205,133,63),
                                "Pink" => array(255,192,203),
                                "Plum" => array(221,160,221),
                                "PowderBlue" => array(176,224,230),
                                "Purple" => array(128,0,128),
                                "Red" => array(255,0,0),
                                "RosyBrown" => array(188,143,143),
                                "RoyalBlue" => array(65,105,225),
                                "SaddleBrown" => array(139,69,19),
                                "Salmon" => array(250,128,114),
                                "SandyBrown" => array(244,164,96),
                                "SeaGreen" => array(46,139,87),
                                "Seashell" => array(255,245,238),
                                "Sienna" => array(160,82,45),
                                "Silver" => array(192,192,192),
                                "SkyBlue" => array(135,206,235),
                                "SlateBlue" => array(106,90,205),
                                "SlateGray" => array(112,128,144),
                                "Snow" => array(255,250,250),
                                "SpringGreen" => array(0,255,127),
                                "SteelBlue" => array(70,130,180),
                                "Tan" => array(210,180,140),
                                "Teal" => array(0,128,128),
                                "Thistle" => array(216,191,216),
                                "Tomato" => array(255,99,71),
                                "Turquoise" => array(64,224,208),
                                "Violet" => array(238,130,238),
                                "Wheat" => array(245,222,179),
                                "White" => array(255,255,255),
                                "WhiteSmoke" => array(245,245,245),
                                "Yellow" => array(255,255,0),
                                "YellowGreen" => array(154,205,50)
                        );
                        
                        
    /**
    * Translates a userdefined color specification into an array of RGB integer values.
    * 
    * Several formats can be handled. HTML like hexadecimal colors like #f0ff00, 
    * names colors, arrays of RGB integer values and strings with percentage values
    * like %100,%50,%20. If the format is unknown black gets returned [0, 0, 0].
    * 
    * @var      mixed   Color in various formats: #f0f0f0, %20,%100,%0, 
    *                   named - black, white..., [int 0 - 255, int 0 - 255, int 0 - 255]
    * @return   array   RGB color [int red, int green, int blue]
    * @access   public
    * @see      $colornames, HTMLColor2RGB(), PercentageColor2RGB(), NamedColor2RGB()
    */                        
    function color2RGB($color) {
    
        if (is_array($color)) {
            
            // looks good...
            if (3 == count($color)) {
            
                // check the range
                foreach ($color as $k => $v) {
                    if ($v < 0)
                        $color[$k] = 0;
                    else if ($v > 255) 
                        $color[$k] = 255;
                    else 
                        $color[$k] = (int)$v;
               }
                
               return $color;        
            }
                

            // unknown format - return black
            return array(0, 0 , 0);
        }

        // #f0f0f0            
        if ("#" == $color{0})
            return $this->HTMLColor2RGB($color);

        // %50,%100,%50
        if ("%" == $color{0})
            return $this->PercentageColor2RGB($color);

        // might be a color name            
        return $this->NamedColor2RGB($color);
    } // end func color2RGB     
    
    
    /**
    * Allocates a color in the given image.
    * 
    * Userdefined color specifications get translated into 
    * an array of rgb values.
    *
    * @param    resource    Image handle
    * @param    mixed       (Userdefined) color specification
    * @return   resource    Image color handle
    * @see      color2RGB()
    * @access   public
    */  
    function allocateColor(&$img, $color) {
        
        $color = $this->color2RGB($color);
        
        return ImageColorAllocate($img, $color[0], $color[1], $color[2]);
    } // end func allocateColor                  
    
    /**
    * Returns the RGB integer values of an HTML like hexadecimal color like #00ff00.
    *
    * @param    string  HTML like hexadecimal color like #00ff00
    * @return   array   [int red, int green, int blue],
    *                   returns black [0, 0, 0] for invalid strings.
    * @access   public
    */
    function HTMLColor2RGB($color) {
        if (strlen($color) != 7)
            return array(0, 0, 0);

        return array(
                        hexdec(substr($color, 1, 2)),
                        hexdec(substr($color, 3, 2)),
                        hexdec(substr($color, 5, 2))
                    );
    } // end func HTMLColor2RGB    
    
    
    /**
    * Returns the RGB interger values of a named color, [0,0,0] if unknown.
    *
    * The class variable $colornames is used to resolve
    * the color names. Modify it if neccessary. 
    *
    * @param    string  Case insensitive color name.
    * @return   array   [int red, int green, int blue], 
    *                   returns black [0, 0, 0] if the color is unknown.
    * @access   public
    * @see      $colornames
    */
    function NamedColor2RGB($color) {
        $color = strtolower($color);
        
        if (!isset($this->colornames[$color]))
            return array(0, 0, 0);
          
        return $this->colornames[$color];
    } // end func NamedColor2RGB 
    
    
    /**
    * Returns the RGB integer values of a color specified by a "percentage string" like "%50,%20,%100". 
    *
    * @param    string
    * @return   array   [int red, int green, int blue]
    * @access   public
    */
    function PercentageColor2RGB($color) {
        
        // split the string %50,%20,%100 by ,
        $color = explode(",", $color);        
                
        foreach ($color as $k => $v) {
        
            // remove the trailing percentage sign %
            $v = (int)substr($v, 1);
            
            // range checks
            if ($v >= 100) {
                $color[$k] = 255;
            } else if ($v <= 0) {
                $color[$k] = 0;
            } else {
                $color[$k] = (int)(2.55 * $v);
            }
            
        } 

        return $color;
    } // end func PercentageColor2RGB
    
    
} // end class ColorHelper
?>