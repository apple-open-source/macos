[comment {-*- tcl -*- doctools = digest_header.inc}]
[include common/trf_version.inc]
[manpage_begin [vset digest] n [vset trf_version]]
[titledesc "Message digest \"[vset digest]\""]
[include common/trf_header.inc]
[description]

The command [cmd [vset digest]] is one of several message digests
provided by the package [package trf]. See [syscmd trf-intro] for an
overview of the whole package.

[para]

[list_begin definitions]

[call [cmd [vset digest]] [opt [arg options...]] [opt [arg data]]]

The options listed below are understood by the digest if and only if
the digest is [term attached] to a channel.

See section [sectref {IMMEDIATE versus ATTACHED}] for an explanation
of the term [term attached].

[list_begin definitions]
[include digest/options.inc]
[list_end]
[nl]

The options listed below are always understood by the digest,

[term attached] versus [term immediate] does not matter. See section
[sectref {IMMEDIATE versus ATTACHED}] for explanations of these two
terms.


[list_begin definitions]
[include common/options.inc]
[list_end]
[list_end]
