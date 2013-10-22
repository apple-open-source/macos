use Perl6::Say;

package Wax::Floor;
use Class::Std;
{
    my %name_of      :ATTR( :get<name> :set<name> );
    my %patent_of    :ATTR( get => 'patent' );

    sub BUILD {
        my ($self, $ident, $arg_ref) = @_;

        $name_of{$ident}   = $arg_ref->{name};
        $patent_of{$ident} = $arg_ref->{patent};
    }

    sub describe :CUMULATIVE {
        my ($self) = @_;

        ::say "The floor wax $name_of{ident $self} (patent: $patent_of{ident $self})";
    }

    sub features :CUMULATIVE {
        return ('Long-lasting', 'Non-toxic', 'Polymer-based');
    }

    sub active_ingredients :CUMULATIVE(BASE FIRST) {
        return "Wax: paradichlorobenzene,  hydrogen peroxide, cyanoacrylate\n";
    }
}

package Topping::Dessert;
use Class::Std;
{
    my (%name_of, %flavour_of) :ATTRS;

    sub BUILD {
        my ($self, $ident, $arg_ref) = @_;
        
        $name_of{$ident}    = $arg_ref->{name};
        $flavour_of{$ident} = $arg_ref->{flavour};
    }

    sub describe :CUMULATIVE {
        my ($self) = @_;

        ::say "The dessert topping $name_of{ident $self} ",
              "with that great $flavour_of{ident $self} taste!";
    }

    sub features :CUMULATIVE {
        return ('Multi-purpose', 'Time-saving', 'Easy-to-use');
    }

    sub active_ingredients:CUMULATIVE(BASE FIRST) {
        return "Topping: sodium hypochlorite, isobutyl ketone, ethylene glycol\n";
    }
}

package Shimmer;
use base qw( Wax::Floor  Topping::Dessert );
use Class::Std;
{
    my %name_of    :ATTR;
    my %patent_of  :ATTR;

    sub BUILD {
        my ($self, $ident, $arg_ref) = @_;
        
        $name_of{$ident}   = $arg_ref->{name};
        $patent_of{$ident} = $arg_ref->{patent};
    }

    sub describe :CUMULATIVE {
        my ($self) = @_;

        ::say "New $name_of{ident $self} (patent: $patent_of{ident $self}). Combining...";
    }

    sub features :CUMULATIVE {
        return ('Multi-purpose', 'Time-saving', 'Easy-to-use');
    }

    sub active_ingredients:CUMULATIVE(BASE FIRST) {
        return "Binder: aromatic hydrocarbons, xylene, methyl mercaptan\n";
    }

    sub DEMOLISH {
        ::say 'Good-bye cruel world!';
    }

    sub as_str : STRINGIFY { return "SHIMMER!!!!!"; }
    sub as_bool : BOOLIFY NUMERIFY { return 0; }
}

my $product 
    = Shimmer->new({ name=>'Shimmer', patent=>1562516251, flavour=>'Vanilla'});

print "As string:  $product\n";
print "As number:  ", 0+$product, "\n";
print "As boolean: ", $product ? "true\n" : "false\n";

$product->describe();

my @features = Shimmer->features();
::say "Shimmer is the @features alternative!";

my $ingredients = $product->active_ingredients();
print "Contains:\n$ingredients";
print "From ", 0+$ingredients, " sources:\n";
print map {"\t$_\n"} keys %$ingredients;
use Data::Dumper 'Dumper';
warn Dumper \%$ingredients;

my $obj = Shimmer->new({patent=>12345, name=>'Shimmer'});
print "Patent: ", $obj->get_patent(), "\n";
print "Name:   ", $obj->get_name(), "\n";
$obj->set_name("Glimmer");
print "Name:   ", $obj->get_name(), "\n";

eval { $obj->set_patent(98765) } or print $@;
eval { $obj->set_name() } or print $@;

