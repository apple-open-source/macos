package Library::OReilly;
use Class::Std;
{
    my %book;   # Not an attribute hash; shared storage for class

    sub add_book {
        my ($class, $title, $arg_ref) = @_; 
        $book{$title} = { Publisher=>q{O'Reilly}, %{$arg_ref} };
    }

    # Book titles accumulate throughout the libraries...
    sub titles :CUMULATIVE {
        return map { "$_ (O'Reilly)"} keys %book;
    }
}

package Library::Manning;
use Class::Std;
{
    my %book;   # Not an attribute hash; shared storage for class

    sub add_book {
        my ($class, $title, $arg_ref) = @_; 
        $book{$title} = { Publisher=>q{Manning}, %{$arg_ref} };
    }

    # Book titles accumulate throughout the libraries...
    sub titles :CUMULATIVE {
        return map { "$_ (Manning)"} keys %book;
    }
}

package Library::All;
use base qw( Library::OReilly  Library::Manning);

package main;

Library::OReilly->add_book(
    'Programming Perl' => { ISBN=>596000278, year=>2000 }
);

Library::Manning->add_book(
    'Object Oriented Perl' => { ISBN=>1884777791, year=>2000 }
);

print join "\n", Library::All->titles();


