<?php
//
// +----------------------------------------------------------------------+
// | PHP version 4.0                                                      |
// +----------------------------------------------------------------------+
// | Copyright (c) 1997-2001 The PHP Group                                |
// +----------------------------------------------------------------------+
// | This source file is subject to version 2.02 of the PHP license,      |
// | that is bundled with this package in the file LICENSE, and is        |
// | available at through the world-wide-web at                           |
// | http://www.php.net/license/2_02.txt.                                 |
// | If you did not receive a copy of the PHP license and are unable to   |
// | obtain it through the world-wide-web, please send a note to          |
// | license@php.net so we can mail you a copy immediately.               |
// +----------------------------------------------------------------------+
// | Authors: Richard Heyes <richard.heyes@heyes-computing.net>           |
// |          Chuck Hagenbuch <chuck@horde.org>                           |
// +----------------------------------------------------------------------+

/**
 * RFC 822 Email address list validation Utility
 *
 * @author  Richard Heyes <richard.heyes@heyes-computing.net>
 * @author  Chuck Hagenbuch <chuck@horde.org>
 * @version $Revision: 1.1.1.1 $
 */
class Mail_RFC822 {

    /**
     * The address being parsed by the RFC822 object.
     * @var string $address
     */
    var $address = '';
    
    /**
     * The default domain to use for unqualified addresses.
     * @var string $default_domain
     */
    var $default_domain = 'localhost';
    
    /**
     * Should we return a nested array showing groups, or flatten everything?
     * @var boolean $nestGroups
     */
    var $nestGroups = true;
    
    /**
     * Whether or not to validate atoms for non-ascii characters.
     * @var boolean $validate
     */
    var $validate = true;
    
    /**
     * The array of raw addresses built up as we parse.
     * @var array $addresses
     */
    var $addresses = array();
    
    /**
     * The final array of parsed address information that we build up.
     * @var array $structure
     */
    var $structure = array();
    
    /**
     * The current error message, if any.
     * @var string $error
     */
    var $error = null;
    
    /**
     * An internal counter/pointer.
     * @var integer $index
     */
    var $index = null;
    
    /**
     * The number of groups that have been found in the address list.
     * @var integer $num_groups
     * @access public
     */
    var $num_groups = 0;
    
    /**
     * A variable so that we can tell whether or not we're inside a
     * Mail_RFC822 object.
     * @var boolean $mailRFC822
     */
    var $mailRFC822 = true;


    /**
     * Sets up the object. The address must either be set here or when
     * calling parseAddressList(). One or the other.
     *
     * @access public
     * @param $address                    The address(es) to validate.
     * @param $default_domain  (Optional) Default domain/host etc. If not supplied, will be set to localhost.
     * @param $nest_groups     (Optional) Whether to return the structure with groups nested for easier viewing.
     * @param $validate        (Optional) Whether to validate atoms. Turn this off if you need to run addresses through before encoding the personal names, for instance.
     * 
     * @return object Mail_RFC822 A new Mail_RFC822 object.
     */
    function Mail_RFC822($address = null, $default_domain = null, $nest_groups = null, $validate = null)
    {
        if (isset($address))        $this->address        = $address;
        if (isset($default_domain)) $this->default_domain = $default_domain;
        if (isset($nest_groups))    $this->nestGroups     = $nest_groups;
        if (isset($validate))       $this->validate       = $validate;
    }


    /**
     * Starts the whole process. The address must either be set here
     * or when creating the object. One or the other.
     *
     * @access public
     * @param $address                    The address(es) to validate.
     * @param $default_domain  (Optional) Default domain/host etc.
     * @param $nest_groups     (Optional) Whether to return the structure with groups nested for easier viewing.
     * @param $validate        (Optional) Whether to validate atoms. Turn this off if you need to run addresses through before encoding the personal names, for instance.
     * 
     * @return array A structured array of addresses.
     */
    function parseAddressList($address = null, $default_domain = null, $nest_groups = null, $validate = null)
    {
        if (!isset($this->mailRFC822)) {
            $obj = new Mail_RFC822($address, $default_domain, $nest_groups, $validate);
            return $obj->parseAddressList();
        }

        if (isset($address))        $this->address        = $address;
        if (isset($default_domain)) $this->default_domain = $default_domain;
        if (isset($nest_groups))    $this->nestGroups     = $nest_groups;
        if (isset($validate))       $this->validate       = $validate;

        $this->structure  = array();
        $this->addresses  = array();
        $this->error      = null;
        $this->index      = null;

        if (!$this->splitAddresses($this->address) || isset($this->error)) {
            return (array)$this->error;
        }

        for ($i = 0; $i < count($this->addresses); $i++){
            if (($return = $this->validateAddress($this->addresses[$i])) === false
                || isset($this->error)) {
                return (array)$this->error;
            }
            
            if (!$this->nestGroups) {
                $this->structure = array_merge($this->structure, $return);
            } else {
                $this->structure[] = $return;
            }
        }
        
        return $this->structure;
    }
    
    /**
     * Splits an address into seperate addresses.
     * 
     * @access private
     * @param $address The addresses to split.
     * @return boolean Success or failure.
     */
    function splitAddresses($address = '')
    {

        if ($this->isGroup($address) && !isset($this->error)) {
            $split_char = ';';
            $is_group   = true;
        } elseif (!isset($this->error)) {
            $split_char = ',';
            $is_group   = false;
        } elseif (isset($this->error)) {
            return false;
        }
        
        // Split the string based on the above ten or so lines.
        $parts  = explode($split_char, $address);
        $string = $this->splitCheck($parts, $split_char);

        // If a group...
        if ($is_group) {
            // If $string does not contain a colon outside of
            // brackets/quotes etc then something's fubar.
            
            // First check there's a colon at all:
            if (strpos($string, ':') === false) {
                $this->error = 'Invalid address: ' . $string;
                return false;
            }

            // Now check it's outside of brackets/quotes:
            if (!$this->splitCheck(explode(':', $string), ':'))
                return false;

            // We must have a group at this point, so increase the counter:
            $this->num_groups++;
        }
        
        // $string now contains the first full address/group.
        // Add to the addresses array.
        $this->addresses[] = array(
                                   'address' => trim($string),
                                   'group'   => $is_group
                                   );

        // Remove the now stored address from the initial line, the +1
        // is to account for the explode character.
        $address = trim(substr($address, strlen($string) + 1));
        
        // If the next char is a comma and this was a group, then
        // there are more addresses, otherwise, if there are any more
        // chars, then there is another address.
        if ($is_group && substr($address, 0, 1) == ','){
            $address = trim(substr($address, 1));
            $this->splitAddresses($address);
            return true;
            
        } elseif (strlen($address) > 0) {
            $this->splitAddresses($address);
            return true;
        } else {
            return true;
        }
        
        // If you got here then something's off
        return false;
    }
    
    /**
     * Checks for a group at the start of the string.
     * 
     * @access private
     * @param $address The address to check.
     * @return boolean Whether or not there is a group at the start of the string.
     */
    function isGroup($address)
    {
        // First comma not in quotes, angles or escaped:
        $parts  = explode(',', $address);
        $string = $this->splitCheck($parts, ',');
        
        // Now we have the first address, we can reliably check for a
        // group by searching for a colon that's not escaped or in
        // quotes or angle brackets.
        if (count($parts = explode(':', $string)) > 1) {
            $string2 = $this->splitCheck($parts, ':');
            return ($string2 !== $string);
        } else {
            return false;
        }
    }
    
    /**
     * A common function that will check an exploded string.
     * 
     * @access private
     * @param $parts The exloded string.
     * @param $char  The char that was exploded on.
     * @return mixed False if the string contains unclosed quotes/brackets, or the string on success.
     */
    function splitCheck($parts, $char)
    {
        $string = $parts[0];
        
        for ($i = 0; $i < count($parts); $i++) {
            if ($this->hasUnclosedQuotes($string)
                || $this->hasUnclosedBrackets($string, '<>')
                || $this->hasUnclosedBrackets($string, '[]')
                || substr($string, -1) == '\\') {
                if (isset($parts[$i + 1])) {
                    $string = $string . $char . $parts[$i + 1];
                } else {
                    $this->error = 'Invalid address spec. Unclosed bracket or quotes';
                    return false;
                }
            } else {
                $this->index = $i;
                break;
            }
        }
        
        return $string;
    }
    
    /**
     * Checks if a string has an unclosed quotes or not.
     * 
     * @access private
     * @param $string The string to check.
     * @return boolean True if there are unclosed quotes inside the string, false otherwise.
     */
    function hasUnclosedQuotes($string)
    {
        $string     = explode('"', $string);
        $string_cnt = count($string);
        
        for ($i = 0; $i < (count($string) - 1); $i++)
            if (substr($string[$i], -1) == '\\')
                $string_cnt--;
        
        return ($string_cnt % 2 === 0);
    }
    
    /**
     * Checks if a string has an unclosed brackets or not. IMPORTANT:
     * This function handles both angle brackets and square brackets;
     * 
     * @access private
     * @param $string The string to check.
     * @param $chars  The characters to check for.
     * @return boolean True if there are unclosed brackets inside the string, false otherwise.
     */
    function hasUnclosedBrackets($string, $chars)
    {
        $num_angle_start = substr_count($string, $chars[0]);
        $num_angle_end   = substr_count($string, $chars[1]);
        
        $this->hasUnclosedBracketsSub($string, $num_angle_start, $chars[0]);
        $this->hasUnclosedBracketsSub($string, $num_angle_end, $chars[1]);
        
        if ($num_angle_start < $num_angle_end) {
            $this->error = 'Invalid address spec. Unmatched quote or bracket (' . $chars . ')';
            return false;
        } else {
            return ($num_angle_start > $num_angle_end);
        }
    }
    
    /**
     * Sub function that is used only by hasUnclosedBrackets().
     * 
     * @access private
     * @param $string The string to check.
     * @param $num    The number of occurences.
     * @param $char   The character to count.
     * @return integer The number of occurences of $char in $string, adjusted for backslashes.
     */
    function hasUnclosedBracketsSub($string, &$num, $char)
    {
        $parts = explode($char, $string);
        for ($i = 0; $i < count($parts); $i++){
            if (substr($parts[$i], -1) == '\\' || $this->hasUnclosedQuotes($parts[$i]))
                $num--;
            if (isset($parts[$i + 1]))
                $parts[$i + 1] = $parts[$i] . $char . $parts[$i + 1];
        }
        
        return $num;
    }
    
    /**
     * Function to begin checking the address.
     *
     * @access private
     * @param $address The address to validate.
     * @return mixed False on failure, or a structured array of address information on success.
     */
    function validateAddress($address)
    {
        $is_group = false;
        
        if ($address['group']) {
            $is_group = true;
            
            // Get the group part of the name
            $parts     = explode(':', $address['address']);
            $groupname = $this->splitCheck($parts, ':');
            $structure = array();
            
            // And validate the group part of the name.
            if (!$this->validatePhrase($groupname)){
                $this->error = 'Group name did not validate.';
                return false;
            } else {
                // Don't include groups if we are not nesting
                // them. This avoids returning invalid addresses.
                if ($this->nestGroups) {
                    $structure = new stdClass;
                    $structure->groupname = $groupname;
                }
            }
            
            $address['address'] = ltrim(substr($address['address'], strlen($groupname . ':')));
        }
        
        // If a group then split on comma and put into an array.
        // Otherwise, Just put the whole address in an array.
        if ($is_group) {
            while (strlen($address['address']) > 0) {
                $parts       = explode(',', $address['address']);
                $addresses[] = $this->splitCheck($parts, ',');
                $address['address'] = trim(substr($address['address'], strlen(end($addresses) . ',')));
            }
        } else {
            $addresses[] = $address['address'];
        }
        
        // Check that $addresses is set, if address like this:
        // Groupname:;
        // Then errors were appearing.
        if (!isset($addresses)){
            $this->error[] = 'Empty group.';
            return false;
        }
        
        for ($i = 0; $i < count($addresses); $i++) {
            $addresses[$i] = trim($addresses[$i]);
        }
        
        // Validate each mailbox.
        // Format could be one of: name <geezer@domain.com>
        //                         geezer@domain.com
        //                         geezer
        // ... or any other format valid by RFC 822.
        array_walk($addresses, array($this, 'validateMailbox'));
        
        // Nested format
        if ($this->nestGroups) {
            if ($is_group) {
                $structure->addresses = $addresses;
            } else {
                $structure = $addresses[0];
            }

        // Flat format
        } else {
            if ($is_group) {
                $structure = array_merge($structure, $addresses);
            } else {
                $structure = $addresses;
            }
        }
        
        return $structure;
    }
    
    /**
     * Function to validate a phrase.
     *
     * @access private
     * @param $phrase The phrase to check.
     * @return boolean Success or failure.
     */
    function validatePhrase($phrase)
    {
        // Splits on one or more Tab or space.
        $parts = preg_split('/[ \\x09]+/', $phrase, -1, PREG_SPLIT_NO_EMPTY);
        
        $phrase_parts = array();
        while (count($parts) > 0){
            $phrase_parts[] = $this->splitCheck($parts, ' ');
            for ($i = 0; $i < $this->index + 1; $i++)
                array_shift($parts);
        }
        
        for ($i = 0; $i < count($phrase_parts); $i++) {
            // If quoted string:
            if (substr($phrase_parts[$i], 0, 1) == '"') {
                if (!$this->validateQuotedString($phrase_parts[$i]))
                    return false;
                continue;
            }
            
            // Otherwise it's an atom:
            if (!$this->validateAtom($phrase_parts[$i])) return false;
        }
        
        return true;
    }
    
    /**
     * Function to validate an atom which from rfc822 is:
     * atom = 1*<any CHAR except specials, SPACE and CTLs>
     * 
     * If validation ($this->validate) has been turned off, then
     * validateAtom() doesn't actually check anything. This is so that you
     * can split a list of addresses up before encoding personal names
     * (umlauts, etc.), for example.
     * 
     * @access private
     * @param $atom The string to check.
     * @return boolean Success or failure.
     */
    function validateAtom($atom)
    {
    if (!$this->validate) {
        // Validation has been turned off; assume the atom is okay.
        return true;
    }
    
        // Check for any char from ASCII 0 - ASCII 127
        if (!preg_match('/^[\\x00-\\x7E]+$/i', $atom, $matches)) {
            return false;
    }
        
        // Check for specials:
        if (preg_match('/[][()<>@,;\\:". ]/', $atom)) {
            return false;
    }
        
        // Check for control characters (ASCII 0-31):
        if (preg_match('/[\\x00-\\x1F]+/', $atom)) {
            return false;
    }
        
        return true;
    }
    
    /**
     * Function to validate quoted string, which is:
     * quoted-string = <"> *(qtext/quoted-pair) <">
     * 
     * @access private
     * @param $qstring The string to check
     * @return boolean Success or failure.
     */
    function validateQuotedString($qstring)
    {
        // Leading and trailing "
        $qstring = substr($qstring, 1, -1);
        
        // Perform check.
        return !(preg_match('/(.)[\x0D\\\\"]/', $qstring, $matches) && $matches[1] != '\\');
    }
    
    /**
     * Function to validate a mailbox, which is:
     * mailbox =   addr-spec         ; simple address
     *           / phrase route-addr ; name and route-addr
     * 
     * @access private
     * @param $mailbox The string to check.
     * @return boolean Success or failure.
     */
    function validateMailbox(&$mailbox)
    {
        // A couple of defaults.
        $phrase = '';
        
        // Check for name + route-addr
        if (substr($mailbox, -1) == '>' && substr($mailbox, 0, 1) != '<') {
            $parts  = explode('<', $mailbox);
            $name   = $this->splitCheck($parts, '<');
            
            $phrase     = trim($name);
            $route_addr = trim(substr($mailbox, strlen($name.'<'), -1));
            
            if ($this->validatePhrase($phrase) === false || ($route_addr = $this->validateRouteAddr($route_addr)) === false)
                return false;
            
        // Only got addr-spec
        } else {
            // First snip angle brackets if present.
            if (substr($mailbox,0,1) == '<' && substr($mailbox,-1) == '>')
                $addr_spec = substr($mailbox,1,-1);
            else
                $addr_spec = $mailbox;
            
            if (($addr_spec = $this->validateAddrSpec($addr_spec)) === false)
                return false;
        }
        
        // Construct the object that will be returned.
        $mbox = new stdClass();
        $phrase !== '' ? $mbox->personal = $phrase : '';
        
        if (isset($route_addr)) {
            $mbox->mailbox = $route_addr['local_part'];
            $mbox->host    = $route_addr['domain'];
            $route_addr['adl'] !== '' ? $mbox->adl = $route_addr['adl'] : '';
        } else {
            $mbox->mailbox = $addr_spec['local_part'];
            $mbox->host    = $addr_spec['domain'];
        }
        
        $mailbox = $mbox;
        return true;
    }
    
    /**
     * This function validates a route-addr which is:
     * route-addr = "<" [route] addr-spec ">"
     *
     * Angle brackets have already been removed at the point of
     * getting to this function.
     * 
     * @access private
     * @param $route_addr The string to check.
     * @return mixed False on failure, or an array containing validated address/route information on success.
     */
    function validateRouteAddr($route_addr)
    {
        // Check for colon.
        if (strpos($route_addr, ':') !== false) {
            $parts = explode(':', $route_addr);
            $route = $this->splitCheck($parts, ':');
        } else {
            $route = $route_addr;
        }
        
        // If $route is same as $route_addr then the colon was in
        // quotes or brackets or, of course, non existent.
        if ($route === $route_addr){
            unset($route);
            $addr_spec = $route_addr;
            if (($addr_spec = $this->validateAddrSpec($addr_spec)) === false) {
                return false;
            }
        } else {
            // Validate route part.
            if (($route = $this->validateRoute($route)) === false) {
                return false;
            }
            
            $addr_spec = substr($route_addr, strlen($route . ':'));
            
            // Validate addr-spec part.
            if (($addr_spec = $this->validateAddrSpec($addr_spec)) === false) {
                return false;
            }
        }
        
        if (isset($route)) {
            $return['adl'] = $route;
        } else {
            $return['adl'] = '';
        }
        
        $return = array_merge($return, $addr_spec);
        return $return;
    }
    
    /**
     * Function to validate a route, which is:
     * route = 1#("@" domain) ":"
     * 
     * @access private
     * @param $route The string to check.
     * @return mixed False on failure, or the validated $route on success.
     */
    function validateRoute($route)
    {
        // Split on comma.
        $domains = explode(',', trim($route));
        
        for ($i = 0; $i < count($domains); $i++) {
            $domains[$i] = str_replace('@', '', trim($domains[$i]));
            if (!$this->validateDomain($domains[$i])) return false;
        }
        
        return $route;
    }
    
    /**
     * Function to validate a domain, though this is not quite what
     * you expect of a strict internet domain.
     *
     * domain = sub-domain *("." sub-domain)
     * 
     * @access private
     * @param $domain The string to check.
     * @return mixed False on failure, or the validated domain on success.
     */
    function validateDomain($domain)
    {
        // Note the different use of $subdomains and $sub_domains                        
        $subdomains = explode('.', $domain);
        
        while (count($subdomains) > 0) {
            $sub_domains[] = $this->splitCheck($subdomains, '.');
            for ($i = 0; $i < $this->index + 1; $i++)
                array_shift($subdomains);
        }
        
        for ($i = 0; $i < count($sub_domains); $i++) {
            if (!$this->validateSubdomain(trim($sub_domains[$i])))
                return false;
        }
        
        // Managed to get here, so return input.
        return $domain;
    }
    
    /**
     * Function to validate a subdomain:
     *   subdomain = domain-ref / domain-literal
     * 
     * @access private
     * @param $subdomain The string to check.
     * @return boolean Success or failure.
     */
    function validateSubdomain($subdomain)
    {
        if (preg_match('|^\[(.*)]$|', $subdomain, $arr)){
            if (!$this->validateDliteral($arr[1])) return false;
        } else {
            if (!$this->validateAtom($subdomain)) return false;
        }
        
        // Got here, so return successful.
        return true;
    }
    
    /**
     * Function to validate a domain literal:
     *   domain-literal =  "[" *(dtext / quoted-pair) "]"
     * 
     * @access private
     * @param $dliteral The string to check.
     * @return boolean Success or failure.
     */
    function validateDliteral($dliteral)
    {
        return !preg_match('/(.)[][\x0D\\\\]/', $dliteral, $matches) && $matches[1] != '\\';
    }
    
    /**
     * Function to validate an addr-spec.
     *
     * addr-spec = local-part "@" domain
     * 
     * @access private
     * @param $addr_spec The string to check.
     * @return mixed False on failure, or the validated addr-spec on success.
     */
    function validateAddrSpec($addr_spec)
    {
        $addr_spec = trim($addr_spec);
        
        // Split on @ sign if there is one.
        if (strpos($addr_spec, '@') !== false) {
            $parts      = explode('@', $addr_spec);
            $local_part = $this->splitCheck($parts, '@');
            $domain     = substr($addr_spec, strlen($local_part . '@'));
            
        // No @ sign so assume the default domain.
        } else {
            $local_part = $addr_spec;
            $domain     = $this->default_domain;
        }
        
        if (($local_part = $this->validateLocalPart($local_part)) === false) return false;
        if (($domain     = $this->validateDomain($domain)) === false) return false;
        
        // Got here so return successful.
        return array('local_part' => $local_part, 'domain' => $domain);
    }
    
    /**
     * Function to validate the local part of an address:
     *   local-part = word *("." word)
     * 
     * @access private
     * @param $local_part
     * @return mixed False on failure, or the validated local part on success.
     */
    function validateLocalPart($local_part)
    {
        $parts = explode('.', $local_part);
        
        // Split the local_part into words.
        while (count($parts) > 0){
            $words[] = $this->splitCheck($parts, '.');
            for ($i = 0; $i < $this->index + 1; $i++) {
                array_shift($parts);
            }
        }
        
        // Validate each word.
        for ($i = 0; $i < count($words); $i++) {
            if ($this->validatePhrase(trim($words[$i])) === false) return false;
        }
        
        // Managed to get here, so return the input.
        return $local_part;
    }
    
}
?>
