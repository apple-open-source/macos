#!/usr/bin/perl

use strict;
use CPAN;
use File::Basename ();
use File::Copy ();
use File::stat ();
use Getopt::Long ();
use IO::File;

my $Modules = 'Modules';
my $PerlLicense = <<EOF;
Licensed under the same terms as Perl:
http://perldoc.perl.org/perlartistic.html
http://perldoc.perl.org/perlgpl.html
EOF

my %modules = (
    'Algorithm-Diff-1.1902' => {
	copyright => 'Parts Copyright (c) 2000-2004 Ned Konz.  All rights reserved.  Parts by Tye McQueen.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'B-Hooks-EndOfScope-0.09' => {
	copyright => '',
	license => 'Perl',
	licensefile => 'B-Hooks-EndOfScope-0.09/LICENSE',
    },
    'B-Hooks-OP-Check-0.19' => {
	copyright => 'Copyright (c) 2008 Florian Ragwitz',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Class-Accessor-Chained-0.01' => {
	copyright => 'Copyright (C) 2003 Richard Clamp.  All Rights Reserved.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Class-Accessor-Grouped-0.10003' => {
	copyright => 'Copyright (c) 2006-2010 Matt S. Trout <mst@shadowcatsystems.co.uk>',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Class-C3-Componentised-1.001000' => {
	copyright => 'Copyright 2008 - 2011 Adam Kennedy.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Class-Inspector-1.25' => {
	copyright => 'Copyright 2002 - 2011 Adam Kennedy.',
	license => 'Perl',
	licensefile => 'Class-Inspector-1.25/LICENSE',
    },
    'Class-Load-0.12' => {
	copyright => '',
	license => 'Perl',
	licensefile => 'Class-Load-0.12/LICENSE',
    },
    'Class-Singleton-1.4' => {
	copyright => 'Copyright Andy Wardley 1998-2007. All Rights Reserved.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Class-XSAccessor-1.12' => {
	copyright => 'Copyright (C) 2008, 2009, 2010, 2011 by Steffen Mueller',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Clone-0.31' => {
	copyright => 'Copyright 2001 Ray Finch.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Config-Any-0.23' => {
	copyright => 'Copyright (c) 2006, Portugal Telecom "http://www.sapo.pt/". All rights reserved. Portions copyright 2007, Joel Bernstein "<rataxis@cpan.org>".',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Context-Preserve-0.01' => {
	copyright => 'Copyright (c) 2008 Infinity Interactive.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'DBIx-Class-0.08195' => {
	copyright => <<'EOC',
Copyright (c) 2005 - 2010 the DBIx::Class "AUTHOR" and "CONTRIBUTORS"
AUTHOR
    mst: Matt S. Trout <mst@shadowcatsystems.co.uk>
    (I mostly consider myself "project founder" these days but the AUTHOR
    heading is traditional :)

CONTRIBUTORS
    abraxxa: Alexander Hartmaier <abraxxa@cpan.org>
    acca: Alexander Kuznetsov <acca@cpan.org>
    aherzog: Adam Herzog <adam@herzogdesigns.com>
    Alexander Keusch <cpan@keusch.at>
    alnewkirk: Al Newkirk <we@ana.im>
    amiri: Amiri Barksdale <amiri@metalabel.com>
    amoore: Andrew Moore <amoore@cpan.org>
    andyg: Andy Grundman <andy@hybridized.org>
    ank: Andres Kievsky
    arc: Aaron Crane <arc@cpan.org>
    arcanez: Justin Hunter <justin.d.hunter@gmail.com>
    ash: Ash Berlin <ash@cpan.org>
    bert: Norbert Csongradi <bert@cpan.org>
    blblack: Brandon L. Black <blblack@gmail.com>
    bluefeet: Aran Deltac <bluefeet@cpan.org>
    bphillips: Brian Phillips <bphillips@cpan.org>
    boghead: Bryan Beeley <cpan@beeley.org>
    bricas: Brian Cassidy <bricas@cpan.org>
    brunov: Bruno Vecchi <vecchi.b@gmail.com>
    caelum: Rafael Kitover <rkitover@cpan.org>
    caldrin: Maik Hentsche <maik.hentsche@amd.com>
    castaway: Jess Robinson
    claco: Christopher H. Laco
    clkao: CL Kao
    da5id: David Jack Olrik <djo@cpan.org>
    debolaz: Anders Nor Berle <berle@cpan.org>
    dew: Dan Thomas <dan@godders.org>
    dkubb: Dan Kubb <dan.kubb-cpan@onautopilot.com>
    dnm: Justin Wheeler <jwheeler@datademons.com>
    dpetrov: Dimitar Petrov <mitakaa@gmail.com>
    dwc: Daniel Westermann-Clark <danieltwc@cpan.org>
    dyfrgi: Michael Leuchtenburg <michael@slashhome.org>
    felliott: Fitz Elliott <fitz.elliott@gmail.com>
    freetime: Bill Moseley <moseley@hank.org>
    frew: Arthur Axel "fREW" Schmidt <frioux@gmail.com>
    goraxe: Gordon Irving <goraxe@cpan.org>
    gphat: Cory G Watson <gphat@cpan.org>
    Grant Street Group <http://www.grantstreet.com/>
    groditi: Guillermo Roditi <groditi@cpan.org>
    Haarg: Graham Knop <haarg@haarg.org>
    hobbs: Andrew Rodland <arodland@cpan.org>
    ilmari: Dagfinn Ilmari Mannsåker <ilmari@ilmari.org>
    initself: Mike Baas <mike@initselftech.com>
    ironcamel: Naveed Massjouni <naveedm9@gmail.com>
    jawnsy: Jonathan Yu <jawnsy@cpan.org>
    jasonmay: Jason May <jason.a.may@gmail.com>
    jesper: Jesper Krogh
    jgoulah: John Goulah <jgoulah@cpan.org>
    jguenther: Justin Guenther <jguenther@cpan.org>
    jhannah: Jay Hannah <jay@jays.net>
    jnapiorkowski: John Napiorkowski <jjn1056@yahoo.com>
    jon: Jon Schutz <jjschutz@cpan.org>
    jshirley: J. Shirley <jshirley@gmail.com>
    kaare: Kaare Rasmussen
    konobi: Scott McWhirter
    littlesavage: Alexey Illarionov <littlesavage@orionet.ru>
    lukes: Luke Saunders <luke.saunders@gmail.com>
    marcus: Marcus Ramberg <mramberg@cpan.org>
    mattlaw: Matt Lawrence
    mattp: Matt Phillips <mattp@cpan.org>
    michaelr: Michael Reddick <michael.reddick@gmail.com>
    milki: Jonathan Chu <milki@rescomp.berkeley.edu>
    ned: Neil de Carteret
    nigel: Nigel Metheringham <nigelm@cpan.org>
    ningu: David Kamholz <dkamholz@cpan.org>
    Nniuq: Ron "Quinn" Straight" <quinnfazigu@gmail.org>
    norbi: Norbert Buchmuller <norbi@nix.hu>
    nuba: Nuba Princigalli <nuba@cpan.org>
    Numa: Dan Sully <daniel@cpan.org>
    ovid: Curtis "Ovid" Poe <ovid@cpan.org>
    oyse: Øystein Torget <oystein.torget@dnv.com>
    paulm: Paul Makepeace
    penguin: K J Cheetham
    perigrin: Chris Prather <chris@prather.org>
    peter: Peter Collingbourne <peter@pcc.me.uk>
    phaylon: Robert Sedlacek <phaylon@dunkelheit.at>
    plu: Johannes Plunien <plu@cpan.org>
    Possum: Daniel LeWarne <possum@cpan.org>
    quicksilver: Jules Bean
    rafl: Florian Ragwitz <rafl@debian.org>
    rainboxx: Matthias Dietrich <perl@rb.ly>
    rbo: Robert Bohne <rbo@cpan.org>
    rbuels: Robert Buels <rmb32@cornell.edu>
    rdj: Ryan D Johnson <ryan@innerfence.com>
    ribasushi: Peter Rabbitson <ribasushi@cpan.org>
    rjbs: Ricardo Signes <rjbs@cpan.org>
    robkinyon: Rob Kinyon <rkinyon@cpan.org>
    Robert Olson <bob@rdolson.org>
    Roman: Roman Filippov <romanf@cpan.org>
    Sadrak: Felix Antonius Wilhelm Ostmann <sadrak@cpan.org>
    sc_: Just Another Perl Hacker
    scotty: Scotty Allen <scotty@scottyallen.com>
    semifor: Marc Mims <marc@questright.com>
    solomon: Jared Johnson <jaredj@nmgi.com>
    spb: Stephen Bennett <stephen@freenode.net>
    Squeeks <squeek@cpan.org>
    sszabo: Stephan Szabo <sszabo@bigpanda.com>
    talexb: Alex Beamish <talexb@gmail.com>
    tamias: Ronald J Kimball <rjk@tamias.net>
    teejay : Aaron Trevena <teejay@cpan.org>
    Todd Lipcon
    Tom Hukins
    tonvoon: Ton Voon <tonvoon@cpan.org>
    triode: Pete Gamache <gamache@cpan.org>
    typester: Daisuke Murase <typester@cpan.org>
    victori: Victor Igumnov <victori@cpan.org>
    wdh: Will Hawes
    willert: Sebastian Willert <willert@cpan.org>
    wreis: Wallace Reis <wreis@cpan.org>
    yrlnry: Mark Jason Dominus <mjd@plover.com>
    zamolxes: Bogdan Lucaciu <bogdan@wiz.ro>
EOC
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Data-Compare-1.22' => {
	copyright => 'Copyright (c) 1999-2001 Fabien Tassin. All rights reserved.  Some parts copyright 2003 - 2010 David Cantrell.',
	license => 'Perl',
	licensefilelist => [
	    'Data-Compare-1.22/ARTISTIC.txt',
	    'Data-Compare-1.22/GPL2.txt',
	],
    },
    'Data-Dumper-Concise-2.020' => {
	copyright => <<'EOC',
Copyright (c) 2010 the Data::Dumper::Concise AUTHOR and CONTRIBUTORS:
AUTHOR
    mst - Matt S. Trout <mst@shadowcat.co.uk>
head1 CONTRIBUTORS
    frew - Arthur Axel "fREW" Schmidt <frioux@gmail.com>
EOC
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Data-Page-2.02' => {
	copyright => 'Copyright (C) 2000-8, Leon Brocard',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'DateTime-0.70' => {
	copyright => '',
	license => 'Artistic License 2.0',
	licensefile => 'DateTime-0.70/LICENSE',
    },
    'DateTime-Locale-0.45' => {
	copyright => <<'EOC',
Copyright (c) 2003 Richard Evans. Copyright (c) 2004-2009 David Rolsky.
All rights reserved.
The locale modules in directory DateTime/Locale have been
generated from data provided by the CLDR project, see
LICENSE.cldr for details on the CLDR data's license.
EOC
	license => 'Perl & CLDR',
	licensefilelist => [
	    'DateTime-Locale-0.45/LICENSE',
	    'DateTime-Locale-0.45/LICENSE.cldr',
	],
    },
    'DateTime-TimeZone-1.41' => {
	copyright => '',
	license => 'Perl',
	licensefile => 'DateTime-TimeZone-1.41/LICENSE',
    },
    'Devel-Caller-2.05' => {
	copyright => 'Copyright (c) 2002, 2003, 2006, 2007, 2008, 2010 Richard Clamp. All Rights Reserved.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Devel-Declare-0.006007' => {
	copyright => <<'EOC',
Copyright (c) 2007, 2008, 2009 Matt S Trout
Copyright (c) 2008, 2009 Florian Ragwitz
stolen_chunk_of_toke.c based on toke.c from the perl core, which is
Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
2000, 2001, 2002, 2003, 2004, 2005, 2006, by Larry Wall and others
EOC
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Devel-PartialDump-0.15' => {
	copyright => 'Copyright (c) 2008, 2009 Yuval Kogman. All rights reserved',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Encode-Locale-1.02' => {
	copyright => '(c) 2010 Gisle Aas `<gisle@aas.no>`.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Eval-Closure-0.06' => {
	copyright => '',
	license => 'Perl',
	licensefile => 'Eval-Closure-0.06/LICENSE',
    },
    'ExtUtils-Depends-0.304' => {
	copyright => '(none)',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'File-Find-Rule-0.33' => {
	copyright => 'Copyright (C) 2002, 2003, 2004, 2006, 2009, 2011 Richard Clamp.  All Rights Reserved.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'File-Listing-6.03' => {
	copyright => 'Copyright 1996-2010, Gisle Aas',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'File-Remove-1.51' => {
	copyright => <<'EOC',
Some parts copyright 2006 - 2011 Adam Kennedy.
Some parts copyright 2004 - 2005 Richard Soderberg.
Original copyright: 1998 by Gabor Egressy, <gabor@vmunix.com>.
EOC
	license => 'Perl',
	licensefile => 'File-Remove-1.51/LICENSE',
    },
    'Getopt-ArgvFile-1.11' => {
	copyright => 'Copyright (c) 1993-2007 Jochen Stenzel. All rights reserved.',
	license => 'Artistic',
	licensestr => <<'EOL',
This module is free software, you can redistribute it and/or modify it
under the terms of the Artistic License distributed with Perl version
5.003 or (at your option) any later version. Please refer to the
Artistic License that came with your Perl distribution for more
details.

The Artistic License should have been included in your distribution of
Perl. It resides in the file named "Artistic" at the top-level of the
Perl source tree (where Perl was downloaded/unpacked - ask your
system administrator if you dont know where this is).  Alternatively,
the current version of the Artistic License distributed with Perl can
be viewed on-line on the World-Wide Web (WWW) from the following URL:

      http://www.perl.com/perl/misc/Artistic.html
EOL
    },
    'Getopt-Long-Descriptive-0.090' => {
	copyright => '',
	license => 'Perl',
	licensefile => 'Getopt-Long-Descriptive-0.090/LICENSE',
    },
    'HTML-Form-6.00' => {
	copyright => 'Copyright 1998-2008 Gisle Aas.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'HTTP-Cookies-6.00' => {
	copyright => 'Copyright 1997-2002 Gisle Aas',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'HTTP-Daemon-6.00' => {
	copyright => 'Copyright 1996-2003, Gisle Aas',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'HTTP-Date-6.00' => {
	copyright => 'Copyright 1995-1999, Gisle Aas',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'HTTP-Message-6.02' => {
	copyright => 'Copyright 1995-2008 Gisle Aas.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'HTTP-Negotiate-6.00' => {
	copyright => 'Copyright 1996,2001 Gisle Aas.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Hash-Merge-0.12' => {
	copyright => 'Copyright (c) 2001 Michael K. Neylon. All rights reserved.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Hook-LexWrap-0.24' => {
	copyright => 'Copyright (c) 2001, Damian Conway. All Rights Reserved.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'LWP-MediaTypes-6.01' => {
	copyright => 'Copyright 1995-1999 Gisle Aas.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Math-Round-0.06' => {
	copyright => 'Copyright (c) 2002 Geoffrey Rommel.  All rights reserved.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Module-Find-0.10' => {
	copyright => 'Copyright (C) 2004-2010 Christian Renz <crenz@web42.com>. All rights reserved.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Module-Pluggable-3.9' => {
	copyright => 'Copyright, 2006 Simon Wistow',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Module-Runtime-0.011' => {
	copyright => 'Copyright (C) 2004, 2006, 2007, 2009, 2010, 2011 Andrew Main (Zefram) <zefram@fysh.org>',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Module-ScanDeps-1.05' => {
	copyright => 'Copyright 2002-2008 by Audrey Tang <autrijus@autrijus.org>; 2005-2009 by Steffen Mueller <smueller@cpan.org>.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Moose-Autobox-0.11' => {
	copyright => 'Copyright (C) 2006-2008 Infinity Interactive, Inc.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'MooseX-AuthorizedMethods-0.006' => {
	copyright => 'Copyright 2010 by Daniel Ruoso et al',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'MooseX-ClassAttribute-0.26' => {
	copyright => '',
	license => 'Artistic License 2.0',
	licensefile => 'MooseX-ClassAttribute-0.26/LICENSE',
    },
    'MooseX-Declare-0.35' => {
	copyright => '',
	license => 'Perl',
	licensefile => 'MooseX-Declare-0.35/LICENSE',
    },
    'MooseX-Getopt-0.37' => {
	copyright => '',
	license => 'Perl',
	licensefile => 'MooseX-Getopt-0.37/LICENSE',
    },
    'MooseX-LazyRequire-0.07' => {
	copyright => '',
	license => 'Perl',
	licensefile => 'MooseX-LazyRequire-0.07/LICENSE',
    },
    'MooseX-Meta-TypeConstraint-ForceCoercion-0.01' => {
	copyright => '',
	license => 'Perl',
	licensefile => 'MooseX-Meta-TypeConstraint-ForceCoercion-0.01/LICENSE',
    },
    'MooseX-Method-Signatures-0.37' => {
	copyright => '',
	license => 'Perl',
	licensefile => 'MooseX-Method-Signatures-0.37/LICENSE',
    },
    'MooseX-NonMoose-0.22' => {
	copyright => '',
	license => 'Perl',
	licensefile => 'MooseX-NonMoose-0.22/LICENSE',
    },
#    'MooseX-POE-0.214' => {
#	copyright => '',
#	license => 'Perl',
#	licensefile => 'MooseX-POE-0.214/LICENSE',
#    },
    'MooseX-Params-Validate-0.16' => {
	copyright => '',
	license => 'Perl',
	licensefile => 'MooseX-Params-Validate-0.16/LICENSE',
    },
    'MooseX-Role-Parameterized-0.27' => {
	copyright => 'Copyright 2007-2010 Infinity Interactive',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'MooseX-Singleton-0.27' => {
	copyright => '',
	license => 'Perl',
	licensefile => 'MooseX-Singleton-0.27/LICENSE',
    },
    'MooseX-Storage-0.30' => {
	copyright => 'Copyright (C) 2007-2008 Infinity Interactive',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'MooseX-StrictConstructor-0.16' => {
	copyright => '',
	license => 'Perl',
	licensefile => 'MooseX-StrictConstructor-0.16/LICENSE',
    },
    'MooseX-Traits-0.11' => {
	copyright => 'Copyright 2008 Infinity Interactive, Inc.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'MooseX-TransactionalMethods-0.008' => {
	copyright => 'Copyright 2010 by Daniel Ruoso et al',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'MooseX-Types-0.30' => {
	copyright => '',
	license => 'Perl',
	licensefile => 'MooseX-Types-0.30/LICENSE',
    },
    'MooseX-Types-DateTime-0.05' => {
	copyright => 'Copyright (c) 2008 Yuval Kogman. All rights reserved',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'MooseX-Types-Structured-0.28' => {
	copyright => '',
	license => 'Perl',
	licensefile => 'MooseX-Types-Structured-0.28/LICENSE',
    },
    'Net-HTTP-6.01' => {
	copyright => 'Copyright 2001-2003 Gisle Aas.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Number-Compare-0.03' => {
	copyright => 'Copyright (C) 2002,2011 Richard Clamp.  All Rights Reserved.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'PAR-1.002' => {
	copyright => 'Copyright 2002-2010 by Audrey Tang <cpan@audreyt.org>.  Copyright 2006-2010 by Steffen Mueller <smueller@cpan.org>.  All rights reserved.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'PAR-Dist-0.47' => {
	copyright => 'Copyright 2003-2009 by Audrey Tang <autrijus@autrijus.org>.  All rights reserved.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'PAR-Packer-1.010' => {
	copyright => 'Copyright 2002-2010 by Audrey Tang <cpan@audreyt.org>.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
#    'POE-1.312' => {
#	copyright => 'POE is Copyright 1998-2009 Rocco Caputo.  All rights reserved.',
#	license => 'Perl',
#	licensestr => $PerlLicense,
#    },
#    'POE-Test-Loops-1.312' => {
#	copyright => 'These tests are Copyright 1998-2009 by Rocco Caputo, Benjamin Smith, and countless contributors.  All rights are reserved.',
#	license => 'Perl',
#	licensefile => 'POE-Test-Loops-1.312/LICENSE',
#    },
    'PPI-1.215' => {
	copyright => 'Copyright 2001 - 2011 Adam Kennedy.',
	license => 'Perl',
	licensefile => 'PPI-1.215/LICENSE',
    },
    'PadWalker-1.92' => {
	copyright => 'Copyright (c) 2000-2010, Robin Houston. All Rights Reserved.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Params-Classify-0.013' => {
	copyright => 'Copyright (C) 2004, 2006, 2007, 2009, 2010 Andrew Main (Zefram) <zefram@fysh.org>.  Copyright (C) 2009, 2010 PhotoBox Ltd',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Params-Validate-1.00' => {
	copyright => '',
	license => 'Artistic License 2.0',
	licensefile => 'Params-Validate-1.00/LICENSE',
    },
    'Parse-Eyapp-1.181' => {
	copyright => 'COPYRIGHT (C) 2006-2008 by Casiano Rodriguez-Leon',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Parse-Method-Signatures-1.003014' => {
	copyright => 'This distribution copyright 2008-2009, Ash Berlin <ash@cpan.org>',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Path-Class-0.24' => {
	copyright => '',
	license => 'Perl',
	licensefile => 'Path-Class-0.24/LICENSE',
    },
    'Perl6-Junction-1.40000' => {
	copyright => 'Copyright 2005, Carl Franks.  All rights reserved.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'SQL-Abstract-1.72' => {
	copyright => 'Copyright (c) 2001-2007 Nathan Wiger <nwiger@cpan.org>. All Rights Reserved.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'String-RewritePrefix-0.006' => {
	copyright => '',
	license => 'Perl',
	licensefile => 'String-RewritePrefix-0.006/LICENSE',
    },
    'Test-Deep-0.108' => {
	copyright => 'Copyright 2003, 2004 by Fergal Daly fergal@esatclear.ie.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Test-Differences-0.61' => {
	copyright => 'Copyright (C) 2008 Curtis "Ovid" Poe',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
#    'Test-NoWarnings-1.03' => {
#	copyright => 'Copyright 2003 - 2007 Fergal Daly.  Some parts copyright 2010 - 2011 Adam Kennedy.',
#	license => 'LGPL',
#	licensefile => 'Test-NoWarnings-1.03/LICENSE',
#    },
    'Test-Object-0.07' => {
	copyright => 'Copyright 2005, 2006 Adam Kennedy. All rights reserved.',
	license => 'Perl',
	licensefile => 'Test-Object-0.07/LICENSE',
    },
    'Test-Output-1.01' => {
	copyright => 'Copyright (C) 2005 Shawn Sorichetti',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Test-SubCalls-1.09' => {
	copyright => 'Copyright 2005 - 2009 Adam Kennedy.',
	license => 'Perl',
	licensefile => 'Test-SubCalls-1.09/LICENSE',
    },
    'Test-Tester-0.107' => {
	copyright => "This module is copyright 2005 Fergal Daly <fergal\@esatclear.ie>, some parts are based on other people's work.",
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Test-Warn-0.23' => {
	copyright => 'Copyright 2002 by Janek Schleicher.  Copyright 2007-2009 by Alexandr Ciornii',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Test-use-ok-0.02' => {
	copyright => 'Copyright 2005, 2006 by Audrey Tang <cpan@audreyt.org>.',
	license => 'MIT',
	licensestr => <<'EOL',
The "MIT" License
  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
EOL
    },
    'Text-Diff-1.41' => {
	copyright => 'Some parts copyright 2009 Adam Kennedy.  Copyright 2001 Barrie Slaymaker. All Rights Reserved.',
	license => 'Perl',
	licensefile => 'Text-Diff-1.41/LICENSE',
    },
    'Text-Glob-0.09' => {
	copyright => 'Copyright (C) 2002, 2003, 2006, 2007 Richard Clamp.  All Rights Reserved.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Tree-DAG_Node-1.06' => {
	copyright => 'Copyright 1998-2001, 2004, 2007 by Sean M. Burke and David Hand.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'Variable-Magic-0.47' => {
	copyright => 'Copyright 2007,2008,2009,2010,2011 Vincent Pit, all rights reserved.',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'WWW-RobotRules-6.01' => {
	copyright => 'Copyright 1995-2009, Gisle Aas.  Copyright 1995, Martijn Koster',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'XML-SAX-Base-1.08' => {
	copyright => 'Copyright 2011 Grant McLean',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'aliased-0.30' => {
	copyright => 'Copyright (C) 2005 by Curtis Poe',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'autobox-2.75' => {
	copyright => 'Copyright (c) 2003-2011 chocolateboy <chocolate@cpan.org>',
	license => 'Perl',
	licensestr => $PerlLicense,
    },
    'namespace-autoclean-0.13' => {
	copyright => '',
	license => 'Perl',
	licensefile => 'namespace-autoclean-0.13/LICENSE',
    },
    'namespace-clean-0.21' => {
	copyright => "This software is copyright (c) 2011 by Robert 'phaylon' Sedlacek.",
	license => 'Perl',
	licensestr => $PerlLicense,
    },
);

my $URLprefix = 'http://search.cpan.org/CPAN/authors/id';

my $opensource; # output opensource copyright and license info
my $write;
Getopt::Long::GetOptions('o' => \$opensource, 'w' => \$write);

sub importDate {
    my($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) = scalar(@_) > 0 ? localtime(shift) : localtime;
    sprintf('%d-%02d-%02d', $year + 1900, $mon + 1, $mday);
}

sub nameVers {
    my $x = shift;
    my @parts = split('-', $x);
    my $vers = pop(@parts);
    (join('-', @parts), $vers)
}

if($opensource) {
    # Legal now says that the full text of the license file is not needed, if
    # it is just one of the standard licenses.  Next time we update CPAN, we
    # should use licensefile more sparingly.
    for my $m (sort(keys(%modules))) {
	print "******** $m ********\n";
	my $h = $modules{$m};
	my @list;
	if(defined($h->{licensefilelist})) {
	    @list = @{$h->{licensefilelist}};
	} elsif(defined($h->{licensefile})) {
	    push(@list, $h->{licensefile});
	}
	die "$m: no copyright\n" unless defined($h->{copyright});
	chomp($h->{copyright});
	if(length($h->{copyright}) > 0) {
	    print "$h->{copyright}\n\n";
	} elsif(scalar(@list) <= 0) {
	    die "$m: copyright empty and no licence file\n";
	}
	if(scalar(@list) > 0) {
	    for(@list) {
		system("cat $_") == 0 or die "\"cat $_\" failed\n";
		print "\n";
	    }
	} else {
	    die "$m: no licensestr\n" unless defined($h->{licensestr});
	    chomp($h->{licensestr});
	    print "$h->{licensestr}\n\n";
	}
    }
    exit(0);
}

CPAN::HandleConfig->load;
CPAN::Shell::setup_output;
CPAN::Index->reload;

my($dist, $name, $vers, $url);
my($OUT, $license, $importDate);
#my @svncmd = qw(svn add);

if($write) {
    if(!-d $Modules) {
	mkdir $Modules or die "Can't mkdir $Modules\n";
    }
} else {
    $OUT = \*STDOUT;
}

for my $m (sort(keys(%modules))) {
    printf "Looking for %s\n", $m;
    my($n, $v) = nameVers($m);
    my $found;
    my $mname = $n;
    $mname =~ s/-/::/g;
    for my $mod (CPAN::Shell->expand("Module", "/$mname/")) {
	$dist = $mod->distribution;
	next unless defined($dist);
	($name, $vers) = nameVers($dist->base_id);
	next unless $name eq $mname;
	print "    Found $name-$vers\n";
	$found = $dist;
	last;
    }
    if(!defined($found)) {
	for my $dist (CPAN::Shell->expand("Distribution", "/\/$n-/")) {
	    ($name, $vers) = nameVers($dist->base_id);
	    next unless $name eq $n;
	    print "    Found $name-$vers\n";
	    $found = $dist;
	    last
	}
	if(!defined($found)) {
	    print "***Can't find $m\n";
	    next;
	}
    }
    $url = $found->pretty_id;
    my $base = $found->base_id;
    $url =~ s/$base/$m/ unless $base eq $m;
    my $a = substr($url, 0, 1);
    my $a2 = substr($url, 0, 2);
    $url = join('/', $URLprefix, $a, $a2, $url);
    if($write) {
	if(!-d "$Modules/$m") {
	    mkdir "$Modules/$m" or die "Can't mkdir $Modules/$m\n";
	}
	File::Copy::syscopy("$m.tar.gz", "$Modules/$m/$m.tar.gz") or die "Can't copy $m.tar.gz: $!\n";
	$OUT = IO::File->new("$Modules/$m/Makefile", 'w');
	if(!defined($OUT)) {
	    warn "***Can't create $Modules/$m/Makefile\n";
	    next;
	}
    } else {
	if(!-f "$m.tar.gz") {
	    warn "No $m.tar.gz\n";
	    next;
	}
	print "    Would copy $m.tar.gz\n";
	print "=== $m/Makefile ===\n";
    }

    print $OUT <<EOF;
NAME = $name
VERSION = $vers

include ../Makefile.inc
EOF
    if($write) {
	undef($OUT);
	$OUT = IO::File->new("$Modules/$m/oss.partial", 'w');
	if(!defined($OUT)) {
	    warn "***Can't create $Modules/$m/oss.partial\n";
	    next;
	}
    } else {
	print "=== $m/oss.partial ===\n";
    }
    my $h = $modules{$m};
    die "$m: no license\n" unless defined($h->{license});
    print $OUT <<EOF;
<dict>
        <key>OpenSourceProject</key>
        <string>$n</string>
        <key>OpenSourceVersion</key>
        <string>$v</string>
        <key>OpenSourceWebsiteURL</key>
        <string>http://search.cpan.org/</string>
        <key>OpenSourceURL</key>
        <string>$url</string>
EOF
    my $stat = File::stat::stat("$m.tar.gz");
    $importDate = defined($h->{date}) ? $h->{date} : importDate($stat->mtime);
    print $OUT <<EOF;
        <key>OpenSourceImportDate</key>
        <string>$importDate</string>
EOF
    print $OUT <<EOF;
        <key>OpenSourceLicense</key>
        <string>$h->{license}</string>
        <key>OpenSourceLicenseFile</key>
        <string>CPAN.txt</string>
</dict>
EOF
    if($write) {
	undef($OUT);
	$license = "$Modules/$m/LICENSE";
    }
    my @list;
    if(defined($h->{licensefilelist})) {
	@list = @{$h->{licensefilelist}};
    } elsif(defined($h->{licensefile})) {
	push(@list, $h->{licensefile});
    }
    if(scalar(@list) > 0) {
	if(!$write) {
	    print "License Files:\n";
	}
	for(@list) {
	    if($write) {
		system("cat $_ >> $license") == 0 or die "\"cat $_ >> $license\" failed\n";
	    } else {
		if(!-f $_) {
		    warn "***No $_\n";
		    next;
		}
		print "    $_\n";
	    }
	}
    } else {
	die "$m: no licensestr\n" unless defined($h->{licensestr});
	if($write) {
	    $OUT = IO::File->new($license, 'w') or die "Can't create $license\n";
	    print $OUT $h->{licensestr};
	    undef($OUT);
	} else {
	    print "=========== License String ==========\n";
	    print $h->{licensestr};
	    print "=====================================\n";
	}
    }
#    if($write) {
#	system(@svncmd, $license, "$Modules/$m/oss.partial") == 0 or die "\"@svncmd $license $Modules/$m/oss.partial\" failed\n";
#    }
}
