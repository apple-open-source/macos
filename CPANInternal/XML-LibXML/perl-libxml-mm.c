/**
 * perl-libxml-mm.c
 * $Id: perl-libxml-mm.c,v 1.1.1.2 2007/10/10 23:04:14 ahuda Exp $
 *
 * Basic concept:
 * perl varies in the implementation of UTF8 handling. this header (together
 * with the c source) implements a few functions, that can be used from within
 * the core module inorder to avoid cascades of c pragmas
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdlib.h>

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#ifdef XML_LIBXML_GDOME_SUPPORT

#include <libgdome/gdome.h>
#include <libgdome/gdome-libxml-util.h>

#endif

#include "perl-libxml-sax.h"

#ifdef __cplusplus
}
#endif

#ifdef XS_WARNINGS
#define xs_warn(string) warn(string) 
/* #define xs_warn(string) fprintf(stderr, string) */
#else
#define xs_warn(string)
#endif

/**
 * this is a wrapper function that does the type evaluation for the 
 * node. this makes the code a little more readable in the .XS
 * 
 * the code is not really portable, but i think we'll avoid some 
 * memory leak problems that way.
 **/
const char*
PmmNodeTypeName( xmlNodePtr elem ){
    const char *name = "XML::LibXML::Node";

    if ( elem != NULL ) {
        switch ( elem->type ) {
        case XML_ELEMENT_NODE:
            name = "XML::LibXML::Element";   
            break;
        case XML_TEXT_NODE:
            name = "XML::LibXML::Text";
            break;
        case XML_COMMENT_NODE:
            name = "XML::LibXML::Comment";
            break;
        case XML_CDATA_SECTION_NODE:
            name = "XML::LibXML::CDATASection";
            break;
        case XML_ATTRIBUTE_NODE:
            name = "XML::LibXML::Attr"; 
            break;
        case XML_DOCUMENT_NODE:
        case XML_HTML_DOCUMENT_NODE:
            name = "XML::LibXML::Document";
            break;
        case XML_DOCUMENT_FRAG_NODE:
            name = "XML::LibXML::DocumentFragment";
            break;
        case XML_NAMESPACE_DECL:
            name = "XML::LibXML::Namespace";
            break;
        case XML_DTD_NODE:
            name = "XML::LibXML::Dtd";
            break;
        case XML_PI_NODE:
            name = "XML::LibXML::PI";
            break;
        default:
            name = "XML::LibXML::Node";
            break;
        };
        return name;
    }
    return "";
}

/*
 * @node: Reference to the node the structure proxies
 * @owner: libxml defines only the document, but not the node owner
 *         (in case of document fragments, they are not the same!)
 * @count: this is the internal reference count!
 * @encoding: this value is missing in libxml2's doc structure
 *
 * Since XML::LibXML will not know, is a certain node is already
 * defined in the perl layer, it can't shurely tell when a node can be
 * safely be removed from the memory. This structure helps to keep
 * track how intense the nodes of a document are used and will not
 * delete the nodes unless they are not refered from somewhere else.
 */
struct _ProxyNode {
    xmlNodePtr node;
    xmlNodePtr owner;
    int count;
    int encoding; 
};

/* helper type for the proxy structure */
typedef struct _ProxyNode ProxyNode;

/* pointer to the proxy structure */
typedef ProxyNode* ProxyNodePtr;

/* this my go only into the header used by the xs */
#define SvPROXYNODE(x) ((ProxyNodePtr)SvIV(SvRV(x)))
#define SvNAMESPACE(x) ((xmlNsPtr)SvIV(SvRV(x)))

#define PmmREFCNT(node)      node->count
#define PmmREFCNT_inc(node)  node->count++
#define PmmNODE(thenode)     thenode->node
#define PmmOWNER(node)       node->owner
#define PmmOWNERPO(node)     ((node && PmmOWNER(node)) ? (ProxyNodePtr)PmmOWNER(node)->_private : node)

#define PmmENCODING(node)    node->encoding
#define PmmNodeEncoding(node) ((ProxyNodePtr)(node->_private))->encoding
#define PmmDocEncoding(node) (node->charset)
/* creates a new proxy node from a given node. this function is aware
 * about the fact that a node may already has a proxy structure.
 */
ProxyNodePtr
PmmNewNode(xmlNodePtr node)
{
    ProxyNodePtr proxy = NULL;

    if ( node == NULL ) {
        xs_warn( "PmmNewNode: no node found\n" );
        return NULL;
    }

    if ( node->_private == NULL ) {
        /* proxy = (ProxyNodePtr)malloc(sizeof(struct _ProxyNode));  */
        Newc(0, proxy, 1, ProxyNode, ProxyNode);
        if (proxy != NULL) {
            proxy->node  = node;
            proxy->owner   = NULL;
            proxy->count   = 0;
            proxy->encoding= 0;
            node->_private = (void*) proxy;
        }
    }
    else {
        proxy = (ProxyNodePtr)node->_private;
    }

    return proxy;
}

ProxyNodePtr
PmmNewFragment(xmlDocPtr doc) 
{
    ProxyNodePtr retval = NULL;
    xmlNodePtr frag = NULL;

    xs_warn("PmmNewFragment: new frag\n");
    frag   = xmlNewDocFragment( doc );
    retval = PmmNewNode(frag);
    /* fprintf(stderr, "REFCNT NOT incremented on frag: 0x%08.8X\n", retval); */

    if ( doc != NULL ) {
        xs_warn("PmmNewFragment: inc document\n");
        /* under rare circumstances _private is not set correctly? */
        if ( doc->_private != NULL ) {
            xs_warn("PmmNewFragment:   doc->_private being incremented!\n");
            PmmREFCNT_inc(((ProxyNodePtr)doc->_private));
            /* fprintf(stderr, "REFCNT incremented on doc: 0x%08.8X\n", doc->_private); */
        }
        retval->owner = (xmlNodePtr)doc;
    }

    return retval;
}

/* frees the node if nessecary. this method is aware, that libxml2
 * has several diffrent nodetypes.
 */
void
PmmFreeNode( xmlNodePtr node )
{  
    switch( node->type ) {
    case XML_DOCUMENT_NODE:
    case XML_HTML_DOCUMENT_NODE:
        xs_warn("PmmFreeNode: XML_DOCUMENT_NODE\n");
        xmlFreeDoc( (xmlDocPtr) node );
        break;
    case XML_ATTRIBUTE_NODE:
        xs_warn("PmmFreeNode: XML_ATTRIBUTE_NODE\n");
        if ( node->parent == NULL ) {
            xs_warn( "PmmFreeNode:   free node!\n");
            node->ns = NULL;
            xmlFreeProp( (xmlAttrPtr) node );
        }
        break;
    case XML_DTD_NODE:
        if ( node->doc != NULL ) {
            if ( node->doc->extSubset != (xmlDtdPtr)node 
                 && node->doc->intSubset != (xmlDtdPtr)node ) {
                xs_warn( "PmmFreeNode: XML_DTD_NODE\n");
                node->doc = NULL;
                xmlFreeDtd( (xmlDtdPtr)node );
            }
        } else {
            xs_warn( "PmmFreeNode: XML_DTD_NODE (no doc)\n");
            xmlFreeDtd( (xmlDtdPtr)node );
        }
        break;
    case XML_DOCUMENT_FRAG_NODE:
        xs_warn("PmmFreeNode: XML_DOCUMENT_FRAG_NODE\n");
    default:
        xs_warn( "PmmFreeNode: normal node\n" );
        xmlFreeNode( node);
        break;
    }
}

/* decrements the proxy counter. if the counter becomes zero or less,
   this method will free the proxy node. If the node is part of a
   subtree, PmmREFCNT_dec will fix the reference counts and delete
   the subtree if it is not required any more.
 */
int
PmmREFCNT_dec( ProxyNodePtr node ) 
{ 
    xmlNodePtr libnode = NULL;
    ProxyNodePtr owner = NULL;  
    int retval = 0;

    if ( node != NULL ) {
        retval = PmmREFCNT(node)--;
	/* fprintf(stderr, "REFCNT on 0x%08.8X decremented to %d\n", node, PmmREFCNT(node)); */
        if ( PmmREFCNT(node) < 0 )
            warn( "PmmREFCNT_dec: REFCNT decremented below 0!" );
        if ( PmmREFCNT(node) <= 0 ) {
            xs_warn( "PmmREFCNT_dec: NODE DELETION\n" );

            libnode = PmmNODE( node );
            if ( libnode != NULL ) {
                if ( libnode->_private != node ) {
                    xs_warn( "PmmREFCNT_dec:   lost node\n" );
                    libnode = NULL;
                }
                else {
                    libnode->_private = NULL;
                }
            }

            PmmNODE( node ) = NULL;
            if ( PmmOWNER(node) && PmmOWNERPO(node) ) {
                xs_warn( "PmmREFCNT_dec:   DOC NODE!\n" );
                owner = PmmOWNERPO(node);
                PmmOWNER( node ) = NULL;
                if( libnode != NULL && libnode->parent == NULL ) {
                    /* this is required if the node does not directly
                     * belong to the document tree
                     */
                    xs_warn( "PmmREFCNT_dec:     REAL DELETE\n" );
                    PmmFreeNode( libnode );
                }
                xs_warn( "PmmREFCNT_dec:   decrease owner\n" );
                PmmREFCNT_dec( owner );
            }
            else if ( libnode != NULL ) {
                xs_warn( "PmmREFCNT_dec:   STANDALONE REAL DELETE\n" );
                
                PmmFreeNode( libnode );
            }
            Safefree( node );
            /* free( node ); */
        }
    }
    else {
        xs_warn("PmmREFCNT_dec: lost node\n" );
    }
    return retval;
}

/* @node: the node that should be wrapped into a SV
 * @owner: perl instance of the owner node (may be NULL)
 *
 * This function will create a real perl instance of a given node.
 * the function is called directly by the XS layer, to generate a perl
 * instance of the node. All node reference counts are updated within
 * this function. Therefore this function returns a node that can
 * directly be used as output.
 *
 * if @ower is NULL or undefined, the node is ment to be the root node
 * of the tree. this node will later be used as an owner of other
 * nodes.
 */
SV*
PmmNodeToSv( xmlNodePtr node, ProxyNodePtr owner ) 
{
    ProxyNodePtr dfProxy= NULL;
    SV * retval = &PL_sv_undef;
    const char * CLASS = "XML::LibXML::Node";

    if ( node != NULL ) {
        /* find out about the class */
        CLASS = PmmNodeTypeName( node );
        xs_warn("PmmNodeToSv: return new perl node of class:\n");
        xs_warn( CLASS );

        if ( node->_private != NULL ) { 
            dfProxy = PmmNewNode(node);
            /* fprintf(stderr, " at 0x%08.8X\n", dfProxy); */
        }
        else {
            dfProxy = PmmNewNode(node);
            /* fprintf(stderr, " at 0x%08.8X\n", dfProxy); */
            if ( dfProxy != NULL ) {
                if ( owner != NULL ) {
                    dfProxy->owner = PmmNODE( owner );
                    PmmREFCNT_inc( owner );
                    /* fprintf(stderr, "REFCNT incremented on owner: 0x%08.8X\n", owner); */
                }
                else {
                   xs_warn("PmmNodeToSv:   node contains itself (owner==NULL)\n");
                }
            }
            else {
                xs_warn("PmmNodeToSv:   proxy creation failed!\n");
            }
        }

        retval = NEWSV(0,0);
        sv_setref_pv( retval, CLASS, (void*)dfProxy );
        PmmREFCNT_inc(dfProxy); 
        /* fprintf(stderr, "REFCNT incremented on node: 0x%08.8X\n", dfProxy); */

        switch ( node->type ) {
        case XML_DOCUMENT_NODE:
        case XML_HTML_DOCUMENT_NODE:
        case XML_DOCB_DOCUMENT_NODE:
            if ( ((xmlDocPtr)node)->encoding != NULL ) {
                dfProxy->encoding = (int)xmlParseCharEncoding( (const char*)((xmlDocPtr)node)->encoding );
            }
            break;
        default:
            break;
        }
    }
    else {
        xs_warn( "PmmNodeToSv: no node found!\n" );
    }

    return retval;
}

/* This is a little helper, that allows us to set the encoding attr. 
 * after broken transformations 
 * 
 * PP: This function is not used!
 */
void
PmmFixProxyEncoding( ProxyNodePtr dfProxy ) 
{
    xmlNodePtr node = PmmNODE( dfProxy );
    
    if ( node != NULL ) {
        switch ( node->type ) {
        case XML_DOCUMENT_NODE:
        case XML_HTML_DOCUMENT_NODE:
        case XML_DOCB_DOCUMENT_NODE:
            if ( ((xmlDocPtr)node)->encoding != NULL ) {
                dfProxy->encoding = (int)xmlParseCharEncoding( (const char*)((xmlDocPtr)node)->encoding );
            }
            break;
        default:
            dfProxy->encoding = 1;
            break;
        }
    }

}

xmlNodePtr
PmmCloneNode( xmlNodePtr node, int recursive )
{
    xmlNodePtr retval = NULL;
    
    if ( node != NULL ) {
        switch ( node->type ) {
        case XML_ELEMENT_NODE:
	case XML_TEXT_NODE:
	case XML_CDATA_SECTION_NODE:
	case XML_ENTITY_REF_NODE:
	case XML_PI_NODE:
	case XML_COMMENT_NODE:
	case XML_DOCUMENT_FRAG_NODE:
	case XML_ENTITY_DECL: 
	  retval = xmlCopyNode( node, recursive ? 1 : 2 );
	  break;
	case XML_ATTRIBUTE_NODE:
	  retval = (xmlNodePtr) xmlCopyProp( NULL, (xmlAttrPtr) node );
	  break;
        case XML_DOCUMENT_NODE:
	case XML_HTML_DOCUMENT_NODE:
	  retval = (xmlNodePtr) xmlCopyDoc( (xmlDocPtr)node, recursive );
	  break;
        case XML_DOCUMENT_TYPE_NODE:
        case XML_DTD_NODE:
	  retval = (xmlNodePtr) xmlCopyDtd( (xmlDtdPtr)node );
	  break;
        case XML_NAMESPACE_DECL:
	  retval = ( xmlNodePtr ) xmlCopyNamespace( (xmlNsPtr) node );
	  break;
        default:
	  break;
        }
    }

    return retval;
}

/* extracts the libxml2 node from a perl reference
 */

xmlNodePtr
PmmSvNodeExt( SV* perlnode, int copy ) 
{
    xmlNodePtr retval = NULL;
    ProxyNodePtr proxy = NULL;

    if ( perlnode != NULL && perlnode != &PL_sv_undef ) {
/*         if ( sv_derived_from(perlnode, "XML::LibXML::Node") */
/*              && SvPROXYNODE(perlnode) != NULL  ) { */
/*             retval = PmmNODE( SvPROXYNODE(perlnode) ) ; */
/*         } */
        xs_warn("PmmSvNodeExt: perlnode found\n" );
        if ( sv_derived_from(perlnode, "XML::LibXML::Node")  ) {
            proxy = SvPROXYNODE(perlnode);
            if ( proxy != NULL ) {
                xs_warn( "PmmSvNodeExt:   is a xmlNodePtr structure\n" );
                retval = PmmNODE( proxy ) ;
            }

            if ( retval != NULL
                 && ((ProxyNodePtr)retval->_private) != proxy ) {
                xs_warn( "PmmSvNodeExt:   no node in proxy node\n" );
                PmmNODE( proxy ) = NULL;
                retval = NULL;
            }
        }
#ifdef  XML_LIBXML_GDOME_SUPPORT
        else if ( sv_derived_from( perlnode, "XML::GDOME::Node" ) ) {
            GdomeNode* gnode = (GdomeNode*)SvIV((SV*)SvRV( perlnode ));
            if ( gnode == NULL ) {
                warn( "no XML::GDOME data found (datastructure empty)" );    
            }
            else {
                retval = gdome_xml_n_get_xmlNode( gnode );
                if ( retval == NULL ) {
                    xs_warn( "PmmSvNodeExt: no XML::LibXML node found in GDOME object\n" );
                }
                else if ( copy == 1 ) {
                    retval = PmmCloneNode( retval, 1 );
                }
            }
        }
#endif
    }

    return retval;
}

/* extracts the libxml2 owner node from a perl reference
 */
xmlNodePtr
PmmSvOwner( SV* perlnode ) 
{
    xmlNodePtr retval = NULL;
    if ( perlnode != NULL
         && perlnode != &PL_sv_undef
         && SvPROXYNODE(perlnode) != NULL  ) {
        retval = PmmOWNER( SvPROXYNODE(perlnode) );
    }
    return retval;
}

/* reverse to PmmSvOwner(). sets the owner of the current node. this
 * will increase the proxy count of the owner.
 */
SV* 
PmmSetSvOwner( SV* perlnode, SV* extra )
{
    if ( perlnode != NULL && perlnode != &PL_sv_undef ) {        
        PmmOWNER( SvPROXYNODE(perlnode)) = PmmNODE( SvPROXYNODE(extra) );
        PmmREFCNT_inc( SvPROXYNODE(extra) );
        /* fprintf(stderr, "REFCNT incremented on new owner: 0x%08.8X\n", SvPROXYNODE(extra)); */
    }
    return perlnode;
}

void PmmFixOwnerList( xmlNodePtr list, ProxyNodePtr parent );

/**
 * this functions fixes the reference counts for an entire subtree.
 * it is very important to fix an entire subtree after node operations
 * where the documents or the owner node may get changed. this method is
 * aware about nodes that already belong to a certain owner node. 
 *
 * the method uses the internal methods PmmFixNode and PmmChildNodes to
 * do the real updates.
 * 
 * in the worst case this traverses the subtree twice durig a node 
 * operation. this case is only given when the node has to be
 * adopted by the document. Since the ownerdocument and the effective 
 * owner may differ this double traversing makes sense.
 */ 
int
PmmFixOwner( ProxyNodePtr nodetofix, ProxyNodePtr parent ) 
{
    ProxyNodePtr oldParent = NULL;

    if ( nodetofix != NULL ) {
        switch ( PmmNODE(nodetofix)->type ) {
        case XML_ENTITY_DECL:
        case XML_ATTRIBUTE_DECL:
        case XML_NAMESPACE_DECL:
        case XML_ELEMENT_DECL:
        case XML_DOCUMENT_NODE:
            xs_warn( "PmmFixOwner: don't need to fix this type of node\n" );
            return(0);
        default:
            break;
        }

        if ( PmmOWNER(nodetofix) != NULL ) {
            oldParent = PmmOWNERPO(nodetofix);
        }
        
        /* The owner data is only fixed if the node is neither a
         * fragment nor a document. Also no update will happen if
         * the node is already his owner or the owner has not
         * changed during previous operations.
         */
        if( oldParent != parent ) {
            xs_warn( "PmmFixOwner: re-parenting node\n" );
	    /* fprintf(stderr, " 0x%08.8X (%s)\n", nodetofix, PmmNODE(nodetofix)->name); */
            if ( parent && parent != nodetofix ){
                PmmOWNER(nodetofix) = PmmNODE(parent);
                    PmmREFCNT_inc( parent );
                    /* fprintf(stderr, "REFCNT incremented on new parent: 0x%08.8X\n", parent); */
            }
            else {
                PmmOWNER(nodetofix) = NULL;
            }
            
            if ( oldParent != NULL && oldParent != nodetofix )
                PmmREFCNT_dec(oldParent);
            
            if ( PmmNODE(nodetofix)->type != XML_ATTRIBUTE_NODE
                 && PmmNODE(nodetofix)->properties != NULL ) {
                PmmFixOwnerList( (xmlNodePtr)PmmNODE(nodetofix)->properties,
                                 parent );
            }

            if ( parent == NULL || PmmNODE(nodetofix)->parent == NULL ) {
                /* fix to self */
                parent = nodetofix;
            }

            PmmFixOwnerList(PmmNODE(nodetofix)->children, parent);
        }
        else {
            xs_warn( "PmmFixOwner: node doesn't need to get fixed\n" );
        }
        return(1);
    }
    return(0);
}

void
PmmFixOwnerList( xmlNodePtr list, ProxyNodePtr parent )
{
    if ( list != NULL ) {
        xmlNodePtr iterator = list;
        while ( iterator != NULL ) {
            switch ( iterator->type ) {
            case XML_ENTITY_DECL:
            case XML_ATTRIBUTE_DECL:
            case XML_NAMESPACE_DECL:
            case XML_ELEMENT_DECL:
                xs_warn( "PmmFixOwnerList: don't need to fix this type of node\n" );
                iterator = iterator->next;
                continue;
                break;
            default:
                break;
            }

            if ( iterator->_private != NULL ) {
                PmmFixOwner( (ProxyNodePtr)iterator->_private, parent );
            }
            else {
                if ( iterator->type != XML_ATTRIBUTE_NODE
                     &&  iterator->properties != NULL ){
                    PmmFixOwnerList( (xmlNodePtr)iterator->properties, parent );
                }
                PmmFixOwnerList(iterator->children, parent);
            }
            iterator = iterator->next;
        }
    }
}

void
PmmFixOwnerNode( xmlNodePtr node, ProxyNodePtr parent )
{
    if ( node != NULL && parent != NULL ) {
        if ( node->_private != NULL ) {
            xs_warn( "PmmFixOwnerNode: calling PmmFixOwner\n" );
            PmmFixOwner( node->_private, parent );
        }
        else {
            xs_warn( "PmmFixOwnerNode: calling PmmFixOwnerList\n" );
            PmmFixOwnerList(node->children, parent );
        } 
    }
} 

ProxyNodePtr
PmmNewContext(xmlParserCtxtPtr node)
{
    ProxyNodePtr proxy = NULL;

    proxy = (ProxyNodePtr)xmlMalloc(sizeof(ProxyNode));
    if (proxy != NULL) {
        proxy->node  = (xmlNodePtr)node;
        proxy->owner   = NULL;
        proxy->count   = 0;
    }
    else {
        warn( "empty context" );
    }
    return proxy;
}
 
int
PmmContextREFCNT_dec( ProxyNodePtr node ) 
{ 
    xmlParserCtxtPtr libnode = NULL;
    int retval = 0;
    if ( node != NULL ) {
        retval = PmmREFCNT(node)--;
	/* fprintf(stderr, "REFCNT on context 0x%08.8X decremented to %d\n", node, PmmREFCNT(node)); */
        if ( PmmREFCNT(node) <= 0 ) {
            xs_warn( "PmmContextREFCNT_dec: NODE DELETION\n" );
            libnode = (xmlParserCtxtPtr)PmmNODE( node );
            if ( libnode != NULL ) {
                if (libnode->_private != NULL ) {
                    if ( libnode->_private != (void*)node ) {
                        PmmSAXCloseContext( libnode );
                    }
                    else {
                        xmlFree( libnode->_private );
                    }
                    libnode->_private = NULL;
                }
                PmmNODE( node )   = NULL;
                xmlFreeParserCtxt(libnode);
            }
        }
        xmlFree( node );
    }
    return retval;
}

SV*
PmmContextSv( xmlParserCtxtPtr ctxt )
{
    ProxyNodePtr dfProxy= NULL;
    SV * retval = &PL_sv_undef;
    const char * CLASS = "XML::LibXML::ParserContext";

    if ( ctxt != NULL ) {
        dfProxy = PmmNewContext(ctxt);

        retval = NEWSV(0,0);
        sv_setref_pv( retval, CLASS, (void*)dfProxy );
        PmmREFCNT_inc(dfProxy); 
        /* fprintf(stderr, "REFCNT incremented on new context: 0x%08.8X\n", dfProxy); */
    }         
    else {
        xs_warn( "PmmContextSv: no node found!\n" );
    }

    return retval;
}

xmlParserCtxtPtr
PmmSvContext( SV * scalar ) 
{
    xmlParserCtxtPtr retval = NULL;

    if ( scalar != NULL
         && scalar != &PL_sv_undef
         && sv_isa( scalar, "XML::LibXML::ParserContext" )
         && SvPROXYNODE(scalar) != NULL  ) {
        retval = (xmlParserCtxtPtr)PmmNODE( SvPROXYNODE(scalar) );
    }
    else {
        if ( scalar == NULL
             && scalar == &PL_sv_undef ) {
            xs_warn( "PmmSvContext: no scalar!\n" );
        }
        else if ( ! sv_isa( scalar, "XML::LibXML::ParserContext" ) ) {
            xs_warn( "PmmSvContext: bad object\n" );
        }
        else if (SvPROXYNODE(scalar) == NULL) {
            xs_warn( "PmmSvContext: empty object\n" );
        }
        else {
            xs_warn( "PmmSvContext: nothing was wrong!\n");
        }
    }
    return retval;
}

xmlChar*
PmmFastEncodeString( int charset,
                     const xmlChar *string,
                     const xmlChar *encoding ) 
{
    xmlCharEncodingHandlerPtr coder = NULL;
    xmlChar *retval = NULL;
    xmlBufferPtr in = NULL, out = NULL;

    if ( charset == XML_CHAR_ENCODING_UTF8 ) {
        /* warn("use UTF8 for encoding ... %s ", string); */
        return xmlStrdup( string );
    }
    else if ( charset == XML_CHAR_ENCODING_ERROR ){
        /* warn("no standard encoding %s\n", encoding); */
        coder =xmlFindCharEncodingHandler( (const char *)encoding );
    }
    else if ( charset == XML_CHAR_ENCODING_NONE ){
        xs_warn("PmmFastEncodeString: no encoding found\n");
    }
    else {
        /* warn( "use document encoding %s (%d)", encoding, charset ); */
        coder= xmlGetCharEncodingHandler( charset );
    }

    if ( coder != NULL ) {
        xs_warn("PmmFastEncodeString: coding machine found \n");
        in    = xmlBufferCreate();
        out   = xmlBufferCreate();
        xmlBufferCCat( in, (const char *) string );
        if ( xmlCharEncInFunc( coder, out, in ) >= 0 ) {
            retval = xmlStrdup( out->content );
            /* warn( "encoded string is %s" , retval); */
        }
        else {
            /* warn( "b0rked encoiding!\n"); */
        }
        
        xmlBufferFree( in );
        xmlBufferFree( out );
        xmlCharEncCloseFunc( coder );
    }
    return retval;
}

xmlChar*
PmmFastDecodeString( int charset,
                     const xmlChar *string,
                     const xmlChar *encoding) 
{
    xmlCharEncodingHandlerPtr coder = NULL;
    xmlChar *retval = NULL;
    xmlBufferPtr in = NULL, out = NULL;

    if ( charset == XML_CHAR_ENCODING_UTF8 ) {
        return xmlStrdup( string );
    }
    else if ( charset == XML_CHAR_ENCODING_ERROR ){
        coder = xmlFindCharEncodingHandler( (const char *) encoding );
    }
    else if ( charset == XML_CHAR_ENCODING_NONE ){
        xs_warn("PmmFastDecodeString: no encoding found\n");
    }
    else {
        coder= xmlGetCharEncodingHandler( charset );
    }

    if ( coder != NULL ) {
        /* warn( "do encoding %s", string ); */
        in  = xmlBufferCreate();
        out = xmlBufferCreate();
        
        xmlBufferCat( in, string );        
        if ( xmlCharEncOutFunc( coder, out, in ) >= 0 ) {
	  retval = xmlCharStrndup((const char *)xmlBufferContent(out), xmlBufferLength(out));
        }
        else {
            xs_warn("PmmFastEncodeString: decoding error\n");
        }
        
        xmlBufferFree( in );
        xmlBufferFree( out );
        xmlCharEncCloseFunc( coder );
    }
    return retval;
}

/** 
 * encodeString returns an UTF-8 encoded String
 * while the encodig has the name of the encoding of string
 **/ 
xmlChar*
PmmEncodeString( const char *encoding, const xmlChar *string ){
    xmlCharEncoding enc;
    xmlChar *ret = NULL;
    
    if ( string != NULL ) {
        if( encoding != NULL ) {
            xs_warn("PmmEncodeString: encoding to UTF-8 from:\n");
            xs_warn( encoding );
            enc = xmlParseCharEncoding( encoding );
            ret = PmmFastEncodeString( enc, string, (const xmlChar *)encoding );
        }
        else {
            /* if utf-8 is requested we do nothing */
            ret = xmlStrdup( string );
        }
    }
    return ret;
}

/**
 * decodeString returns an $encoding encoded string.
 * while string is an UTF-8 encoded string and 
 * encoding is the coding name
 **/
char*
PmmDecodeString( const char *encoding, const xmlChar *string){
    char *ret=NULL;
    xmlCharEncoding enc;

    if ( string != NULL ) {
        xs_warn( "PmmDecodeString called\n" );
        if( encoding != NULL ) {
            enc = xmlParseCharEncoding( encoding );
            ret = (char*)PmmFastDecodeString( enc, string, (const xmlChar*)encoding );
            xs_warn( "PmmDecodeString done\n" );
        }
        else {
            ret = (char*)xmlStrdup(string);
        }
    }
    return ret;
}


SV*
C2Sv( const xmlChar *string, const xmlChar *encoding )
{
    SV *retval = &PL_sv_undef;
    xmlCharEncoding enc;
    STRLEN len = 0;

    if ( string != NULL ) {
        if ( encoding != NULL ) {
            enc = xmlParseCharEncoding( (const char*)encoding );
        }
        else {
            enc = 0;
        }
        if ( enc == 0 ) {
            /* this happens if the encoding is "" or NULL */
            enc = XML_CHAR_ENCODING_UTF8;
        }

        len = xmlStrlen( string );
        retval = newSVpvn( (const char *)string, xmlStrlen(string) );
   
        if ( enc == XML_CHAR_ENCODING_UTF8 ) {
            /* create an UTF8 string. */       
#ifdef HAVE_UTF8
            xs_warn("C2Sv: set UTF8-SV-flag\n");
            SvUTF8_on(retval);
#endif            
        }
    }

    return retval;
}

xmlChar *
Sv2C( SV* scalar, const xmlChar *encoding )
{
    xmlChar *retval = NULL;

    xs_warn("SV2C: start!\n");
    if ( scalar != NULL && scalar != &PL_sv_undef ) {
        STRLEN len = 0;
        char * t_pv =SvPV(scalar, len);
        xmlChar* ts = NULL;
        xmlChar* string = xmlStrdup((xmlChar*)t_pv);
        if ( xmlStrlen(string) > 0 ) {
            xs_warn( "SV2C:   no undefs\n" );
#ifdef HAVE_UTF8
            xs_warn( "SV2C:   use UTF8\n" );
            if( !DO_UTF8(scalar) && encoding != NULL ) {
#else
            if ( encoding != NULL ) {        
#endif
                xs_warn( "SV2C:   domEncodeString!\n" );
                ts= PmmEncodeString( (const char *)encoding, string );
                xs_warn( "SV2C:   done encoding!\n" );
                if ( string != NULL ) {
                    xmlFree(string);
                }
                string=ts;
            }
        }
             
        retval = xmlStrdup(string);
        if (string != NULL ) {
            xmlFree(string);
        }
    }
    xs_warn("SV2C: end!\n");
    return retval;
}

SV*
nodeC2Sv( const xmlChar * string,  xmlNodePtr refnode )
{
    /* this is a little helper function to avoid to much redundand
       code in LibXML.xs */
    SV* retval = &PL_sv_undef;
    STRLEN len = 0;
    xmlChar * decoded = NULL;

    if ( refnode != NULL ) {
        xmlDocPtr real_doc = refnode->doc;
        if ( real_doc != NULL && real_doc->encoding != NULL ) {
            xs_warn( " encode node !!" );
            /* The following statement is to handle bad
               values set by XML::LibXSLT */

            if ( PmmNodeEncoding(real_doc) == XML_CHAR_ENCODING_NONE ) {
                PmmNodeEncoding(real_doc) = XML_CHAR_ENCODING_UTF8;
            }

            decoded = PmmFastDecodeString( PmmNodeEncoding(real_doc) ,
                                           (const xmlChar *)string,
                                           (const xmlChar*)real_doc->encoding);

            xs_warn( "push decoded string into SV" );
            len = xmlStrlen( decoded );
            retval = newSVpvn( (const char *)decoded, len );
            xmlFree( decoded );

            if ( PmmNodeEncoding( real_doc ) == XML_CHAR_ENCODING_UTF8 ) {
                /* most probably true, since libxml2 always 
                 * sets doc->charset to UTF8, see tree.c:
                 *
                 * The in memory encoding is always UTF8
                 * This field will never change and would
                 * be obsolete if not for binary compatibility.
                 */
#ifdef HAVE_UTF8
                xs_warn("nodeC2Sv: set UTF8-SV-flag\n");
                SvUTF8_on(retval);
#endif            
            }
           
            return retval;
        }
    }

    return C2Sv(string, NULL );
}

xmlChar *
nodeSv2C( SV * scalar, xmlNodePtr refnode )
{
    /* this function requires conditionized compiling, because we
       request a function, that does not exists in earlier versions of
       perl. in this cases the library assumes, all strings are in
       UTF8. if a programmer likes to have the intelligent code, he
       needs to upgrade perl */

    if ( refnode != NULL ) {
        xmlDocPtr real_dom = refnode->doc;
        xs_warn("nodeSv2C: have node!\n");
        if (real_dom != NULL && real_dom->encoding != NULL ) {
            xs_warn("nodeSv2C:   encode string!\n");
            /*  speed things a bit up.... */
            if ( scalar != NULL && scalar != &PL_sv_undef ) {
                STRLEN len = 0;
                char * t_pv =SvPV(scalar, len);
                xmlChar* ts = NULL;
                xmlChar* string = xmlStrdup((xmlChar*)t_pv);
                if ( xmlStrlen(string) > 0 ) {
                    xs_warn( "nodeSv2C:   no undefs\n" );
#ifdef HAVE_UTF8
                    xs_warn( "nodeSv2C:   use UTF8\n" );
                    if( !DO_UTF8(scalar) && real_dom != NULL && real_dom->encoding != NULL )
#else
                    if ( real_dom != NULL && real_dom->encoding != NULL )
#endif
		    {
                        xs_warn( "nodeSv2C:     domEncodeString!\n" );
                        /*  if ( string == NULL || *string == 0 ) warn("string is empty" ); */
                        /* The following statement is to handle bad
                           values set by XML::LibXSLT */
                        if ( PmmNodeEncoding(real_dom) == XML_CHAR_ENCODING_NONE ) {
                            PmmNodeEncoding(real_dom) = XML_CHAR_ENCODING_UTF8;
                        }

                        ts= PmmFastEncodeString( PmmNodeEncoding(real_dom),
                                                 string,
                                                 (const xmlChar*)real_dom->encoding );
                        xs_warn( "nodeSv2C:     done!\n" );
                        if ( string != NULL ) {
                            xmlFree(string);
                        }
                        string=ts;
                    }
                    else {
                        xs_warn( "nodeSv2C:   no encoding set, use UTF8!\n" );
                    }
                }
                /* if ( string == NULL ) warn( "nodeSv2C:     string is NULL\n" ); */
                return string;
            }
            else {
                xs_warn( "nodeSv2C:   return NULL\n" );
                return NULL;
            }
        }
        else {
            xs_warn( "nodeSv2C:   document has no encoding defined! use simple SV extraction\n" );
        }
    }
    xs_warn("nodeSv2C: no encoding !!\n");

    return  Sv2C( scalar, NULL ); 
}

SV * 
PmmNodeToGdomeSv( xmlNodePtr node ) 
{
    SV * retval = &PL_sv_undef;

#ifdef XML_LIBXML_GDOME_SUPPORT
    GdomeNode * gnode = NULL;
    GdomeException exc;
    const char * CLASS = "";

    if ( node != NULL ) {
        gnode = gdome_xml_n_mkref( node );
        if ( gnode != NULL ) {
            switch (gdome_n_nodeType(gnode, &exc)) {
            case GDOME_ELEMENT_NODE:
                CLASS = "XML::GDOME::Element";
                break;
            case GDOME_ATTRIBUTE_NODE:
                CLASS = "XML::GDOME::Attr";
                break;
            case GDOME_TEXT_NODE:
                CLASS = "XML::GDOME::Text"; 
                break;
            case GDOME_CDATA_SECTION_NODE:
                CLASS = "XML::GDOME::CDATASection"; 
                break;
            case GDOME_ENTITY_REFERENCE_NODE:
                CLASS = "XML::GDOME::EntityReference"; 
                break;
            case GDOME_ENTITY_NODE:
                CLASS = "XML::GDOME::Entity"; 
                break;
            case GDOME_PROCESSING_INSTRUCTION_NODE:
                CLASS = "XML::GDOME::ProcessingInstruction"; 
                break;
            case GDOME_COMMENT_NODE:
                CLASS = "XML::GDOME::Comment"; 
                break;
            case GDOME_DOCUMENT_TYPE_NODE:
                CLASS = "XML::GDOME::DocumentType"; 
                break;
            case GDOME_DOCUMENT_FRAGMENT_NODE:
                CLASS = "XML::GDOME::DocumentFragment"; 
                break;
            case GDOME_NOTATION_NODE:
                CLASS = "XML::GDOME::Notation"; 
                break;
            case GDOME_DOCUMENT_NODE:
                CLASS = "XML::GDOME::Document"; 
                break;
            default:
                break;
            }

            retval = NEWSV(0,0);
            sv_setref_pv( retval, CLASS, gnode);
        }
    }
#endif

    return retval;
}
