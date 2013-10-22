package Library::OReilly;
use Class::Std;
{
    my %book;   # Not an attribute hash; shared storage for class

    sub add_book {
        my ($class, $title, $arg_ref) = @_; 
        $book{$title} = { Title=>$title, Publisher=>q{O'Reilly}, %{$arg_ref} };
    }

    # Book titles accumulate throughout the libraries...
    sub titles :CUMULATIVE {
        return map { "$_ (O'Reilly)"} keys %book;
    }

    # Treat every undefined method call as a request for books
    # with titles containing the method name...
    sub AUTOLOAD :CUMULATIVE {

        # Fully qualified name of the desired method
        # is passed in the $AUTOLOAD package variable...
        use vars qw( $AUTOLOAD );               # Placate 'use strict'
        my ($book_title_keyword) =              # Extract book title keyword
            $AUTOLOAD =~ m/ .* :: (.*) /xms;    #    by extracting method name

        # If that name matches any of the book titles, return those titles...
        if (my @matches = grep { /$book_title_keyword/ixms } keys %book) {
            return @book{@matches};
        }

        # Otherwise return no titles...
        return;
    }
    
}

package Library::Manning;
use Class::Std;
{
    my %book;

    sub add_book {
        my ($class, $title, $arg_ref) = @_; 
        $book{$title} = { Title=>$title, Publisher=>q{Manning}, %{$arg_ref} };
    }

    sub titles :CUMULATIVE {
        return map { "$_ (Manning)"} keys %book;
    }
 
    # Treat every undefined method call as a request for books
    # with titles containing the method name...
    sub AUTOLOAD :CUMULATIVE {

        # Fully qualified name of the desired method
        # is passed in the $AUTOLOAD package variable...
        use vars qw( $AUTOLOAD );               # Placate 'use strict'
        my ($book_title_keyword) =              # Extract book title keyword
            $AUTOLOAD =~ m/ .* :: (.*) /xms;    #    by extracting method name

        # If that name matches any of the book titles, return those titles...
        if (my @matches = grep { /$book_title_keyword/ixms } keys %book) {
            return @book{@matches};
        }

        # Otherwise return no titles...
        return;
    }
}

package Library;
use base qw( Library::OReilly  Library::Manning);

package main;

Library::OReilly->add_book(
    'Programming Perl' => { ISBN=>596000278, year=>2000 }
);

Library::Manning->add_book(
    'Object Oriented Perl' => { ISBN=>1884777791, year=>2000 }
);

print join "\n", Library->titles();

print "\n-----------------\n";

use Data::Dumper 'Dumper';
print Dumper( Library->Perl() );
print "\n-----------------\n";

