// Expected format is for a div to contain up to three things:
// 1) a link to the chapter
// 2) an optional disclosure triangle with 'open' and 'closed'
//    attributes indicating what open/closed image to use.
// 3) an optional span next to the chapter link that can be collapsed
//    and expanded.
//
// <div>
//      <formatting_stuff><a href="bogus">Bundles</a></formatting_stuff>
//      <span>...collapsible section...</span>
// </div>

var from_toc_frame = 0;     // Track whether request originated from TOC frame to deal with a Mac IE bug.
var isJavaScriptTOC = 0;    // If this looks like a JavaScript TOC this is set to 1.
var ignore_page_load = 0;   // If user clicks TOC link to load page, don't do display stuff to TOC that's done for next/previous buttons, etc.
var lastSelectedItem;       // So we can change the formatting back to normal.
var lastSelectedColor;      // The color it used to be.
var lastSelectedWeight;     // The font weight it used to be
var lastHighlightedChapterAnchor;       // The chapter link that was highlighted when a page in that chapter was loaded.

function initialize_toc() {
    // Called on page load to setup the page.
    
    // TODO: exclude any browsers that can't hack it.
//    alert('BROWSER: ' + navigator.appName + ' VERSION: ' + navigator.appVersion);

    set_initial_state();
}

function set_initial_state() {    
    // Set action for all links, expand first chapter and collapse the rest.
    for(var i = 0; i < document.links.length; i++) {
        document.links[i].onmousedown = link_action;
    }
    
    var divs = document.getElementsByTagName("DIV");
    if (divs.length) {
        isJavaScriptTOC = 1;
        from_toc_frame = 1;
        
        for (var i = 0; i < divs.length; i++) {
            if (i == 0) {
                expand(divs[i]);
            } else {
                collapse(divs[i]);
            }
        }
        
        from_toc_frame = 0;
    }    
}

function link_action() {
    // Called by a link when clicked.
    // If this is a regular link, open the section if it's closed.
    // If it contains a disclosure triangle, toggle the section.
    
    // Since the page load is being driven by the user clicking on the TOC,
    // we don't want to go through the display stuff we do if the page is
    // being loaded by clicking on links in the content.
    ignore_page_load = 1;
    
    var div = div_parent(this);
    
    if (div && div.className) {
        from_toc_frame = 1;
        
        var triangle = disclosure_image(this);
        if (triangle) {
            toggle(div);
        } else if (div.className == "jtoc_closed") {
            expand(div);
        }  
        
        from_toc_frame = 0; 
    }
}

function toggle(div) {
    // Toggle the div's disclosure status.
    if (div.className) {
        if (div.className == "jtoc_open") {
            collapse(div);
        } else if (div.className == "jtoc_closed") {
            expand(div);
        }
    }
}

function expand_only(div) {
    // Collapse all but the specified section.
    // If div is null, everything is collapsed.
    var divs = document.getElementsByTagName("DIV");
    if (divs.length) {
        for (var i = 0; i < divs.length; i++) {
            var current = divs[i];
 
            if (div && current == div) {
                expand(current);
            } else {
                collapse(current);
            }
        }
    }
}

function expand(div) {
    if (isNetscape()) {
        // Netscape can't deal with setting display to none after initial page load,
        // so skip expanding/collapsing.
        return;
    }
    
    if (div.className != "jtoc_open") {
        div.className = "jtoc_open";
        
        var child = collapsible_child(div);
        if (child) { child.style.display = ""; }
        
        // Skip image changing for Mac IE since some frame-related bug
        // messes that up when things originated from another frame.
        if (from_toc_frame || !isMacIE()) {
            var image = disclosure_image(div);
            if (image) { image.src = image.getAttribute("open"); }
        }
    }
}

function collapse(div) {
    if (isNetscape()) {
        // Netscape can't deal with setting display to none after initial page load,
        // so skip expanding/collapsing.
        return;
    }

    if (div.className != "jtoc_closed") {
        div.className = "jtoc_closed";
        
        var child = collapsible_child(div);
        if (child) { child.style.display = "none"; }
        
        // Skip image changing for Mac IE since some frame-related bug
        // messes that up when things originated from another frame.
        if (from_toc_frame || !isMacIE()) {
            var image = disclosure_image(div);
            if (image) { image.src = image.getAttribute("closed"); }
        }
    }
}

function highlight_chapter_link(div) {
    // Used by page_loaded() to highlight the chapter link when a page is loaded within that chapter.
    // Unbold any previously bolded chapter.
    var anchor = first_content_link(div);
    if (anchor) {
        if (lastHighlightedChapterAnchor) { lastHighlightedChapterAnchor.style.fontWeight = "normal"; }
        lastHighlightedChapterAnchor = anchor;
        anchor.style.fontWeight = "bold"; 
    }    
}

function first_content_link(div) {
    // Get the first anchor with a logicalPath attribute.
    // This is guaranteed to be one of our content links, and not a link used for some other purpose.
    // This is typically used to get the chapter link for a div.  We could consider assigning
    // chapter links a style in Gutenberg.
    var anchors = div.getElementsByTagName("A");

    if (anchors.length) {
        for (var i = 0; i < anchors.length; i++) {
            var anchor = anchors[i];
            var logicalPath = anchor.getAttribute("logicalPath");
            if (logicalPath) {
                return anchor;
            }
        }
    }
}

function div_parent(element) {
    // Find the first parent that is a DIV.
    var div_parent = element.parentNode;
    while (div_parent && div_parent.nodeName != "DIV") {
        div_parent = div_parent.parentNode;
    }
    
    return div_parent;
}

function disclosure_image(div) {
    var images = div.getElementsByTagName("IMG");
    if (images.length) {
        for (var i = 0; i < images.length; i++) {
            var image = images[i];
            var open = image.getAttribute("open")
            if (open) {
                return image;
            }
        }
    }
}

function collapsible_child(div) {
    // Get the first (and should be only) div child.
    var span_nodes = div.getElementsByTagName("SPAN");
    
    if (span_nodes.length) {
        return span_nodes[0];
    }
}

function disclosure_triangle() {
    // The mapping table sets disclosure triangles to call this.
    var parent = div_parent(this);
    if (parent) { toggle(parent); }
    return 0;
}

function selected_div(page_location) {
    // Called by a page on loading, so we can track what page is displayed.
    if (isJavaScriptTOC) {
        var page_suffix = path_suffix(page_location.pathname);
        var all_links = document.links;
        
        for(var i = 0; i < all_links.length; i++) {
            var anchor = all_links[i];
            var anchor_suffix = path_suffix(anchor.getAttribute("HREF"));
            
            if (page_suffix == anchor_suffix) {
                return div_parent(anchor);
            }
        }
    }
}

function page_loaded(page_location, tocInSubdirectories) {
    // Called by a page on loading, so we can track what page is displayed.
    // If there is a link that points to the loaded page, make sure that TOC
    // section is disclosed and turn that link black and bold.
    // tocInSubdirectories is passed through to help with locating TOC files

    if (isJavaScriptTOC) {
        // alert(' it thinks it isJavaScriptTOC');

        var page_suffix = path_suffix(page_location.pathname, tocInSubdirectories);
        var all_links = document.links;
        
        for(var i = 0; i < all_links.length; i++) {
            var anchor = all_links[i];
            var anchor_suffix = path_suffix(anchor.getAttribute("HREF"));

        if (page_suffix == anchor_suffix) {
                if (lastSelectedItem) { lastSelectedItem.style.color = lastSelectedColor; lastSelectedItem.style.fontWeight = lastSelectedWeight }
                
                lastSelectedItem = anchor;
                lastSelectedColor = anchor.style.color;
                lastSelectedWeight = anchor.style.fontWeight;
                
                anchor.style.color = "black";
                anchor.style.fontWeight = "bold";
                
                // If this page load didn't come from the TOC,
                // get the parent section and expand it.
                var parent = div_parent(anchor);

                highlight_chapter_link(parent);

                if (ignore_page_load) {
                    ignore_page_load = 0;
                } else {
                    expand_only(parent);
                }
                break;
            }
        }
    }
}

function path_suffix(path, tocInSubdirectories) {
    // Returns last two path segments as a string: "leaf_dir/filename.html".
    // tocInSubdirectories indicates that the various TOC files are one level down, so we adjust the return path
    var leaf = "";
    var parent = "";
        
    // First split on # to get rid of any anchor at end of path.
    var path_array = path.split('#');
    path = path_array[0];
        
    // Now split apart the path.
    // 3379110: Was using array.pop() function, but it failed in Mac IE.
    path_array = path.split('/');
    var length = path_array.length;
    
    if (length) { leaf = path_array[length - 1]; }
    if (length > 1) { parent = path_array[length - 2]; }
    if (tocInSubdirectories) {
        return '/' + leaf;
    }
    else {return parent + '/' + leaf;}

}

function isMacIE() {
    if (navigator.appName == "Microsoft Internet Explorer") {
        // There are some limitations on MacIE so look for that.
        var regex = /Macintosh/;
        if (regex.test(navigator.appVersion)) {
            return 1;
        }
    }
    
    return 0;
}

function isNetscape() {
    // WebKit shows up as Netscape but doesn't have the same problems, so exclude that case.
    if (navigator.appName == "Netscape") {
        var regex = /AppleWebKit/;
        if (!regex.test(navigator.appVersion)) {
            return 1;
        }
    }
    
    return 0;
}