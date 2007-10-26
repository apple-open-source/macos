package My::TypeTest;

=begin WSDL

_IN blaanyType                 $anyType                is a anyType               
_IN blaanySimpleType           $anySimpleType          is a anySimpleType              
_IN blastring                  $string                 is a string                      
_IN blanormalizedString        $normalizedString       is a normalizedString      
_IN blatoken                   $token                  is a token                      
_IN blaanyUri                  $anyUri                 is a anyUri                      
_IN blalanguage                $language               is a language              
_IN blaName                    $Name                   is a Name                      
_IN blaQName                   $QName                  is a QName                      
_IN blaNCName                  $NCName                 is a NCName                      
_IN blaboolean                 $boolean                is a boolean                      
_IN blafloat                   $float                  is a float                      
_IN bladouble                  $double                 is a double                      
_IN bladecimal                 $decimal                is a decimal                      
_IN blaint                     $int                    is a int                      
_IN blapositiveInteger         $positiveInteger        is a positiveInteger              
_IN blanonPositiveInteger      $nonPositiveInteger     is a nonPositiveInteger    
_IN blanegativeInteger         $negativeInteger        is a negativeInteger              
_IN blanonNegativeInteger      $nonNegativeInteger     is a nonNegativeInteger    
_IN blalong                    $long                   is a long                      
_IN blashort                   $short                  is a short                      
_IN blabyte                    $byte                   is a byte                      
_IN blaunsignedInt             $unsignedInt            is a unsignedInt              
_IN blaunsignedLong            $unsignedLong           is a unsignedLong              
_IN blaunsignedShort           $unsignedShort          is a unsignedShort              
_IN blaunsignedByte            $unsignedByte           is a unsignedByte              
_IN bladuration                $duration               is a duration              
_IN bladateTime                $dateTime               is a dateTime              
_IN blatime                    $time                   is a time                      
_IN bladate                    $date                   is a date                      
_IN blagYearMonth              $gYearMonth             is a gYearMonth              
_IN blagYear                   $gYear                  is a gYear                      
_IN blagMonthDay               $gMonthDay              is a gMonthDay              
_IN blagDay                    $gDay                   is a gDay                      
_IN blagMonth                  $gMonth                 is a gMonth                      
_IN blahexBinary               $hexBinary              is a hexBinary              
_IN blabase64Binary            $base64Binary           is a base64Binary            

=cut

sub testXSDTypes {}

=begin WSDL

_IN foo $My::Foo is my Foo

=cut

sub testComplexTypes {}

=begin WSDL

_IN foo @My::Foo is my Foo
_IN bar @string is a bar

=cut

sub testArrayTypes {}

1;
