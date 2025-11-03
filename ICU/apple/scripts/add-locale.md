# add-locale

Adds new locales by dispersing locale data across the appropriate files.

## Overview

This script takes a JSON file containing locale information and automatically updates multiple components of the ICU project structure:

- Creates CLDR XML files for the new locale
- Updates language name translations in existing CLDR XML files
- Updates ICU source files (uloc.cpp) with synchronized language code arrays
- Updates build configuration files (inputFiles.xcfilelist)
- Provides notifications for test files that may need manual review

## JSON Input Format

The script expects a JSON file with the following structure:

```json
{
    "rdar": "160427611",
    "iso639": "nez",
    "exemplars": {
        "primary": "[a á e é i í o ó u ú b c d f g h j k l ł m n p q r s t v w x {x̣} y z ʼ]",
        "auxiliary": "[ʷ ƛ š {x̂}]",
        "punctuation": "[·]"
    },
    "names" : {
        "ar": "نيز برس",
        "bg": "нез-перс",
        "bn": "নেজপার্স",
        "ca": "Nez Percé",
        "cs": "nez perce",
        "da": "nez perce",
        "de": "Nez Perce",
        "el": "Νεζ πέρσι",
        "en": "Nez Perce",
        "es": "nez perce",
        "es_419": "nez perce",
        "fi": "nez perce",
        "fr": "nez-percé",
        "fr_CA": "nez-percé",
        "gu": "નેઝપર્સ",
        "he": "נזפרס",
        "hi": "नेज़पर्स",
        "hr": "nez perce",
        "hu": "nez perce",
        "id": "Nez Perce",
        "it": "nez perce",
        "ja": "ネズパース語",
        "kk": "нез-перс тілі",
        "kn": "ನೆಜ್‌ಪರ್ಸ್",
        "ko": "네즈퍼스어",
        "lt": "nez perce",
        "ml": "നെസ്പര്സ്",
        "mr": "नेझपर्स",
        "ms": "Nez Perce",
        "nb": "nez perce",
        "nez": "nimipuutímt",
        "nl": "Nez Perce",
        "or": "ନେଜ଼ପର୍ସ",
        "pa": "ਨੇਜ਼ਪਰਸ",
        "pl": "nez perce",
        "pt": "nez perce",
        "pt_PT": "nez perce",
        "ro": "nez perce",
        "ru": "нез-перс",
        "sk": "nez perce",
        "sl": "nez perce",
        "sv": "nez perce",
        "ta": "நெஸ்பர்ஸ்",
        "te": "నెస్‌పర్స్",
        "th": "เนะซปัรซ",
        "tr": "Nez Perce dili",
        "uk": "нез-перс",
        "ur": "نیز پرس",
        "vi": "Tiếng Nez Perce",
        "zh": "内兹珀斯语",
        "zh_Hant": "內茲珀斯文",
        "zh_Hant_HK": "內茲珀斯文"
    }
}
```

### Required Fields

- `rdar`: Radar/ticket number for tracking purposes
- `iso639`: ISO 639 language code (2 or 3 letters)
- `exemplars`: Character sets used by the language
  - `primary`: Main character set
  - `auxiliary`: Additional characters (accented, etc.)
  - `punctuation`: Punctuation marks
- `names`: Language name translations in various locales
  - Must include at least the language's own name (self-reference)
  - Should include English (`en`) translation
  - Can include additional language translations

## Usage

The script must be run from the ICU repository root directory:

```sh
python3 apple/scripts/add-locale.py <json_file>
```

### Example Commands

Add Nez Perce language:
```sh
python3 apple/scripts/add-locale.py /path/to/nez.txt
```

## What the Script Does

### 1. Creates CLDR XML File
Creates `cldr/common/main/{locale_code}.xml` with:
- Language identity information
- Self-referencing language name
- Character exemplar sets (primary, auxiliary, punctuation)

### 2. Updates Language Names
Updates existing CLDR XML files in `cldr/common/main/` to include the new language name translations in alphabetical order.

### 3. Updates ICU Source Code
Modifies `icu/icu4c/source/common/uloc.cpp`:
- Adds language code to `LANGUAGES` array (2-letter codes) in alphabetical order
- Adds language code to `LANGUAGES_3` array (3-letter codes) at synchronized index position
- Maintains proper array correspondence between 2-letter and 3-letter code arrays

### 4. Updates Build Configuration
Updates `icu/icu4c/source/data/inputFiles.xcfilelist` to include entries for all data categories:
- `curr` (currency data)
- `lang` (language data)  
- `locales` (locale data)
- `region` (region data)
- `unit` (unit data)
- `zone` (timezone data)

### 5. Updates structLocale.txt
While this script does not update any of the unit tests, it will update test/testdata/structLocale.txt to add the new language code.

### 6. Identifies Files Needing Manual Review
Provides notifications for test files and build configurations that may require manual updates:
- Test files in `icu/icu4c/source/test/`
- Build configuration files

## Directory Structure Verification

The script includes built-in verification to ensure it's run from the correct location. It checks for the existence of these required paths:
- `cldr/common/main/`
- `icu/icu4c/source/common/uloc.cpp`
- `icu/icu4c/source/data/inputFiles.xcfilelist`
- `apple/scripts/`

If any required paths are missing, the script will exit with an error message.

## Technical Details

### Array Synchronization
The script maintains synchronized arrays in `uloc.cpp`:
- `LANGUAGES` array uses alphabetical ordering for 2-letter language codes
- `LANGUAGES_3` array maintains the same index positions as `LANGUAGES` array (not alphabetical)
- This ensures proper correspondence between 2-letter and 3-letter language codes

### XML Formatting
- Uses proper tab indentation (3 tabs for language entries)
- Maintains consistent formatting with existing CLDR files
- Includes rdar comments for tracking changes

### Alphabetical Insertion
- Correctly inserts new entries in alphabetical order across all file types
- Handles edge cases (insertion at beginning, middle, or end of lists)
- Preserves existing formatting and structure

## Error Handling

The script includes comprehensive error handling for:
- Missing JSON input files
- Invalid directory structure
- Missing required JSON fields
- File permission issues
- Malformed existing files

## Limitations

- Does not handle languages that use 2-character codes correctly
- Test file updates require manual review
- Some build configuration updates may need manual verification
- Script assumes standard ICU project structure
- Does not handle complex CLDR inheritance or alias scenarios
