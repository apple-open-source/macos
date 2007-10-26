%events = (
    start_document         => {},
    processing_instruction => {Target => 'xml-stylesheet',
                               Data   => 'href="style.xml" type="text/xsl"'
                              },

    start_prefix_mapping   => {Prefix => 'foo',
                               NamespaceURI => 'http://localhost/foo'
                              },

    start_element          => {Name       => 'foo:root',
                               LocalName  => 'root',
                               Prefix     => 'foo',
                               Attributes => {}           
                              },

    characters             => {Data => 'i am some text'},
    ignorable_whitespace   => {Data => '          '},
    skipped_entity         => {Name => 'huh'},
    set_document_locator   => {Name => 'huh'},
    end_element            => {Name       => 'foo:root',
                               LocalName  => 'root',
                               Prefix     => 'foo'
                              },
    
    end_prefix_mapping     => {Prefix => 'foo',
                               NamespaceURI => 'http://localhost/foo'
                              },
    xml_decl               => {Version => '1.0'},
    start_cdata            => {},
    end_cdata              => {},
    comment                => {Data => 'i am a comment'},
    entity_reference       => {Bogus => 1},
    notation_decl          => {Name         => 'entname',
                               PublicID     => 'huh?'
                              },
    unparsed_entity_decl   => {Name         => 'entname',
                               PublicID     => 'huh?',
                               NotationName => 'notname'
                              },
    element_decl           => {Name  => 'elname',
                               Model => 'huh?',
                              },
    attlist_decl           => {},
    doctype_decl           => {},
    entity_decl            => {},
    attribute_decl         => {ElementName  => 'elname',
                               AttrName     => 'attr',
                              },
    internal_entity_decl   => {Name  => 'entname',
                               Value => 'entavl'
                              },
    external_entity_decl   => {Name     => 'entname',
                               PublicID => 'huh?'
                              },
    resolve_entity         => {},
    start_dtd              => {},
    end_dtd                => {},
    start_entity           => {Name => 'entname'},
    end_entity             => {Name => 'entname'},
    warning                => {Message => 'i warned ye!'},
    error                  => {Message => 'bad things'},
    fatal_error            => {Message => 'et tu brute?'},
    end_document           => {msg => 'parse complete'}
);
