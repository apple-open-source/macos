package Library::OReilly;
use Class::Std;
{
    my %book;   # Not an attribute hash; shared storage for class

    sub add_book {
        my ($class, $title, $book_data) = @_;
        $book{$title} = $book_data;
    }

    # Book titles accumulate throughout the libraries...
    sub titles :CUMULATIVE {
        return keys %book;
    }

    # Treat every undefined method call as a request for books
    # with titles containing the method name...
    sub AUTOMETHOD {
        # Name of the desired method is passed in $_...
        my $book_title = $_;

        # If that name matches any of the book titles, return those titles...
        if (my @matches = grep { /$book_title/ } keys %book) {
            return sub{ @book{@matches} };
        }

        # Otherwise fail...
        return;
    }
}



package Library::All;
use base qw( Library::OReilly );
{
    my %book;

    sub add_book {
        my ($class, $title, $arg_ref) = @_;
        $book{$title} = $arg_ref;
    }

    sub titles :CUMULATIVE {
        return keys %book;
    }

    sub AUTOMETHOD {
        my $book_title = $_;

        if (my @matches = grep { /$book_title/ } keys %book) {
            return sub{ @book{@matches} };
        }

        return;
    }
}


package main;

Library::OReilly->add_book(
    'Programming Perl'     => { ISBN=>596000278, year=>2000 }
);

Library::All->add_book(
    'Object Oriented Perl' => { ISBN=>1884777791, year=>2000 }
);

print join "\n", Library::All->titles();

print "\n-------\n";

use Data::Dumper 'Dumper';
print Dumper(Library::All->Programming());

print "\n-------\n";

print Dumper(Library::All->Python());

