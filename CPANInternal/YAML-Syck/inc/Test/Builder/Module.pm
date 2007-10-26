#line 1
package Test::Builder::Module;

use Test::Builder;

require Exporter;
@ISA = qw(Exporter);

$VERSION = '0.03';

use strict;

# 5.004's Exporter doesn't have export_to_level.
my $_export_to_level = sub {
      my $pkg = shift;
      my $level = shift;
      (undef) = shift;                  # redundant arg
      my $callpkg = caller($level);
      $pkg->export($callpkg, @_);
};


#line 82

sub import {
    my($class) = shift;

    my $test = $class->builder;

    my $caller = caller;

    $test->exported_to($caller);

    $class->import_extra(\@_);
    my(@imports) = $class->_strip_imports(\@_);

    $test->plan(@_);

    $class->$_export_to_level(1, $class, @imports);
}


sub _strip_imports {
    my $class = shift;
    my $list  = shift;

    my @imports = ();
    my @other   = ();
    my $idx = 0;
    while( $idx <= $#{$list} ) {
        my $item = $list->[$idx];

        if( defined $item and $item eq 'import' ) {
            push @imports, @{$list->[$idx+1]};
            $idx++;
        }
        else {
            push @other, $item;
        }

        $idx++;
    }

    @$list = @other;

    return @imports;
}


#line 144

sub import_extra {}


#line 175

sub builder {
    return Test::Builder->new;
}


1;
