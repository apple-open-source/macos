use Test::More;
eval q{ use Test::Spelling };
plan skip_all => 'Test::Spelling is not installed.' if $@;
add_stopwords(map { split /[\s\:\-]+/ } <DATA>);
$ENV{LANG} = 'C';
set_spell_cmd("aspell -l en list") if `which aspell`;
all_pod_files_spelling_ok('lib');

__DATA__
API
Berjon
CPAN
Compat
Dimas
Doran
JSON
Mims
Prather
Wistow
compat
jsonToObj
mst
objToJson
unicode
