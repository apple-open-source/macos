#!/usr/bin/env python3
"""
ICU Locale Addition Script

This script takes a JSON file containing locale information and disperses the changes
across the ICU project structure, including CLDR XML files, ICU source files, and
build configuration files.

Usage: python add_locale.py <json_file>
"""

import json
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Dict, List, Optional, Tuple


class ICULocaleAdder:
    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.cldr_main = project_root / "cldr" / "common" / "main"
        self.icu_source = project_root / "icu" / "icu4c" / "source"
        
    def load_locale_data(self, json_file: Path) -> Dict:
        """Load locale data from JSON file."""
        with open(json_file, 'r', encoding='utf-8') as f:
            return json.load(f)
    
    def create_cldr_xml(self, locale_code: str, locale_data: Dict) -> None:
        """Create the main CLDR XML file for the new locale."""
        xml_file = self.cldr_main / f"{locale_code}.xml"
        
        # Create XML structure
        root = ET.Element("ldml")
        
        # Add DOCTYPE declaration manually since ElementTree doesn't handle it well
        xml_content = '<?xml version="1.0" encoding="UTF-8" ?>\n'
        xml_content += '<!DOCTYPE ldml SYSTEM "../../common/dtd/ldml.dtd">\n'
        xml_content += f'<!-- This is a very minimal file pending more information from {locale_data["names"]["en"]} contributors. -->\n'
        
        # Identity section
        identity = ET.SubElement(root, "identity")
        version = ET.SubElement(identity, "version")
        version.set("number", "$Revision$")
        language = ET.SubElement(identity, "language")
        language.set("type", locale_code)
        
        # Locale display names
        locale_display_names = ET.SubElement(root, "localeDisplayNames")
        languages = ET.SubElement(locale_display_names, "languages")
        lang_elem = ET.SubElement(languages, "language")
        lang_elem.set("type", locale_code)
        lang_elem.text = locale_data["names"][locale_code]
        
        # Characters section
        characters = ET.SubElement(root, "characters")
        
        # Primary exemplars
        primary_exemplars = ET.SubElement(characters, "exemplarCharacters")
        primary_exemplars.text = locale_data["exemplars"]["primary"]
        
        # Auxiliary exemplars
        aux_exemplars = ET.SubElement(characters, "exemplarCharacters")
        aux_exemplars.set("type", "auxiliary")
        aux_exemplars.text = locale_data["exemplars"]["auxiliary"]
        
        # Punctuation exemplars
        punct_exemplars = ET.SubElement(characters, "exemplarCharacters")
        punct_exemplars.set("type", "punctuation")
        punct_exemplars.text = locale_data["exemplars"]["punctuation"]
        
        # Convert to string and add proper formatting
        xml_str = ET.tostring(root, encoding='unicode')
        
        # Add proper indentation
        formatted_xml = self._format_xml(xml_str)
        
        # Write to file
        with open(xml_file, 'w', encoding='utf-8') as f:
            f.write(xml_content + formatted_xml + '\n')
        
        print(f"Created {xml_file}")
    
    def _format_xml(self, xml_str: str) -> str:
        """Add proper indentation to XML."""
        # Parse and reformat the XML with proper indentation
        import xml.dom.minidom
        
        # Parse the XML string
        dom = xml.dom.minidom.parseString(xml_str)
        
        # Format with indentation using tabs
        formatted = dom.toprettyxml(indent='\t')
        
        # Remove the XML declaration line and empty lines
        lines = formatted.split('\n')
        result_lines = []
        
        for line in lines:
            if line.strip() and not line.strip().startswith('<?xml'):
                result_lines.append(line)
        
        return '\n'.join(result_lines)
    
    def update_language_names(self, locale_code: str, locale_data: Dict) -> None:
        """Update language names in all CLDR XML files."""
        rdar_comment = f' <!-- rdar://{locale_data["rdar"]} -->'
        
        for lang_code, display_name in locale_data["names"].items():
            if lang_code == locale_code:
                continue  # Skip self-reference, handled in create_cldr_xml
                
            xml_file = self.cldr_main / f"{lang_code}.xml"
            if not xml_file.exists():
                continue
            
            # Read the file
            with open(xml_file, 'r', encoding='utf-8') as f:
                content = f.read()
            
            # Find the languages section
            languages_start = content.find('<languages>')
            languages_end = content.find('</languages>')
            
            if languages_start == -1 or languages_end == -1:
                continue
            
            # Extract the languages section
            languages_section = content[languages_start:languages_end]
            
            # Find all existing language entries using regex
            lang_pattern = r'<language type="([^"]+)"[^>]*>([^<]*)</language>'
            matches = list(re.finditer(lang_pattern, languages_section))
            
            # Find the correct insertion point alphabetically
            insertion_point = None
            for match in matches:
                existing_code = match.group(1)
                if existing_code > locale_code:
                    insertion_point = match
                    break
            
            if insertion_point:
                # Insert before this match - find the beginning of the line
                match_pos = languages_start + insertion_point.start()
                line_start = content.rfind('\n', 0, match_pos) + 1
                pos = line_start
                new_entry = f'\t\t\t<language type="{locale_code}">{display_name}</language>{rdar_comment}\n'
                new_content = content[:pos] + new_entry + content[pos:]
            else:
                # Insert at the end of the languages section
                pos = languages_end
                new_entry = f'\t\t\t<language type="{locale_code}">{display_name}</language>{rdar_comment}\n\t\t'
                new_content = content[:pos] + new_entry + content[pos:]
            
            # Write back to file
            with open(xml_file, 'w', encoding='utf-8') as f:
                f.write(new_content)
            
            print(f"Updated {xml_file}")
    
    def update_uloc_cpp(self, locale_code: str, locale_data: Dict) -> None:
        """Update the uloc.cpp file to include the new language code in both arrays."""
        uloc_file = self.icu_source / "common" / "uloc.cpp"
        
        with open(uloc_file, 'r', encoding='utf-8') as f:
            content = f.read()
        
        rdar_comment = f'// rdar://{locale_data["rdar"]} add {locale_code}'
        
        # Update LANGUAGES array (2-letter codes) and get the insertion index
        content, insertion_index = self._update_languages_array(content, locale_code, rdar_comment)
        
        # Update LANGUAGES_3 array (3-letter codes) using the same index
        content = self._update_languages_3_array(content, locale_code, rdar_comment, insertion_index)
        
        with open(uloc_file, 'w', encoding='utf-8') as f:
            f.write(content)
        
        print(f"Updated {uloc_file}")
    
    def _find_insertion_point(self, codes: List[str], new_code: str) -> int:
        """Find the correct alphabetical insertion point for a new language code."""
        for i, code in enumerate(codes):
            if code > new_code:
                return i
        return len(codes)
    
    def _update_languages_array(self, content: str, locale_code: str, rdar_comment: str) -> Tuple[str, int]:
        """Update the 2-letter LANGUAGES array and return the insertion index."""
        # Find the LANGUAGES array section
        languages_start = content.find('constexpr const char* LANGUAGES[] = {')
        if languages_start == -1:
            return content, -1
        
        # Find the end of the array (first nullptr)
        null_pos = content.find('nullptr,', languages_start)
        if null_pos == -1:
            return content, -1
        
        # Extract the array content
        array_section = content[languages_start:null_pos]
        
        # Find all language codes in the array using regex
        lang_pattern = r'"([a-z]{2,3})"'
        matches = list(re.finditer(lang_pattern, array_section))
        
        # Find the insertion point and track the index
        insertion_point = None
        insertion_index = 0
        for i, match in enumerate(matches):
            current_code = match.group(1)
            if current_code > locale_code:
                insertion_point = match
                insertion_index = i
                break
        
        if insertion_point:
            # Insert before this match
            pos = languages_start + insertion_point.start()
            new_content = (content[:pos] + 
                          f'"{locale_code}", ' + 
                          content[pos:])
        else:
            # Insert at the end before nullptr
            insertion_index = len(matches)  # Index at the end
            pos = languages_start + null_pos - 1  # Before the last quote/comma
            # Find the last language code line
            last_line_match = None
            for match in matches:
                last_line_match = match
            
            if last_line_match:
                # Find the end of the line containing the last match
                line_end = array_section.find('\n', last_line_match.end())
                if line_end != -1:
                    pos = languages_start + line_end
                    new_content = (content[:pos] + 
                                  f'\n    "{locale_code}",' + 
                                  content[pos:])
                else:
                    new_content = content
            else:
                new_content = content
        
        return new_content, insertion_index
    
    def _update_languages_3_array(self, content: str, locale_code: str, rdar_comment: str, insertion_index: int) -> str:
        """Update the 3-letter LANGUAGES_3 array using the same index as the LANGUAGES array."""
        # Find the LANGUAGES_3 array section
        languages3_start = content.find('constexpr const char* LANGUAGES_3[] = {')
        if languages3_start == -1:
            return content
        
        # Find the end of the array (first nullptr)
        null_pos = content.find('nullptr,', languages3_start)
        if null_pos == -1:
            return content
        
        # Extract the array content
        array_section = content[languages3_start:null_pos]
        
        # Find all language codes in the array using regex
        lang_pattern = r'"([a-z]{3})"'
        matches = list(re.finditer(lang_pattern, array_section))
        
        # Use the insertion_index to find the correct position
        if insertion_index < len(matches):
            # Insert at the specified index position
            target_match = matches[insertion_index]
            pos = languages3_start + target_match.start()
            new_content = (content[:pos] + 
                          f'"{locale_code}", ' + 
                          content[pos:])
        else:
            # Insert at the end before nullptr
            pos = languages3_start + null_pos - 1  # Before the last quote/comma
            # Find the last language code line
            last_line_match = None
            for match in matches:
                last_line_match = match
            
            if last_line_match:
                # Find the end of the line containing the last match
                line_end = array_section.find('\n', last_line_match.end())
                if line_end != -1:
                    pos = languages3_start + line_end
                    new_content = (content[:pos] + 
                                  f'\n    "{locale_code}",' + 
                                  content[pos:])
                else:
                    new_content = content
            else:
                new_content = content
        
        return new_content
    
    def update_input_files_xcfilelist(self, locale_code: str) -> None:
        """Update the inputFiles.xcfilelist to include new locale data files."""
        xcfilelist_file = self.icu_source / "data" / "inputFiles.xcfilelist"
        
        with open(xcfilelist_file, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Data categories that need locale-specific entries
        data_categories = ['curr', 'lang', 'locales', 'region', 'unit', 'zone']
        
        for category in data_categories:
            content = self._update_data_category(content, category, locale_code)
        
        with open(xcfilelist_file, 'w', encoding='utf-8') as f:
            f.write(content)
        
        print(f"Updated {xcfilelist_file}")
    
    def update_struct_locale_txt(self, locale_code: str) -> None:
        """Update the structLocale.txt file to include the new locale in the Languages section."""
        struct_locale_file = self.icu_source / "test" / "testdata" / "structLocale.txt"
        
        with open(struct_locale_file, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Find the Languages section
        languages_start = content.find('Languages{')
        if languages_start == -1:
            print(f"Warning: Languages section not found in {struct_locale_file}")
            return
        
        # Find the end of the Languages section (matching closing brace)
        brace_count = 0
        languages_end = -1
        for i in range(languages_start, len(content)):
            if content[i] == '{':
                brace_count += 1
            elif content[i] == '}':
                brace_count -= 1
                if brace_count == 0:
                    languages_end = i
                    break
        
        if languages_end == -1:
            print(f"Warning: Could not find end of Languages section in {struct_locale_file}")
            return
        
        # Extract the Languages section content
        languages_section = content[languages_start:languages_end]
        
        # Find all existing locale entries using regex
        locale_pattern = r'([a-z_]+)\{""\}'
        matches = list(re.finditer(locale_pattern, languages_section))
        
        # Check if the locale already exists
        existing_codes = [match.group(1) for match in matches]
        if locale_code in existing_codes:
            print(f"Locale {locale_code} already exists in {struct_locale_file}")
            return
        
        # Find the correct insertion point alphabetically
        insertion_point = None
        for match in matches:
            existing_code = match.group(1)
            if existing_code > locale_code:
                insertion_point = match
                break
        
        new_entry = f'{locale_code}{{""}}'
        
        if insertion_point:
            # Insert before this match on its own line
            pos = languages_start + insertion_point.start()
            # Find the beginning of the line containing the insertion point
            line_start = content.rfind('\n', 0, pos)
            if line_start != -1:
                # Get the indentation from the current line
                line_content = content[line_start + 1:pos]
                indent = ''
                for char in line_content:
                    if char in [' ', '\t']:
                        indent += char
                    else:
                        break
                # Insert new entry on its own line with proper indentation
                new_content = content[:line_start + 1] + indent + new_entry + '\n' + content[line_start + 1:]
            else:
                new_content = content[:pos] + new_entry + ' ' + content[pos:]
        else:
            # Insert at the end of the Languages section (before the closing brace)
            if matches:
                # Add after the last existing entry
                last_match = matches[-1]
                pos = languages_start + last_match.end()
                # Find the end of the line containing the last match
                line_end = content.find('\n', pos)
                if line_end != -1:
                    # Insert after the last entry with proper indentation
                    # Get the indentation from the next line (should be closing brace or next entry)
                    next_line_start = line_end + 1
                    next_line_content = content[next_line_start:]
                    indent = ''
                    for char in next_line_content:
                        if char in [' ', '\t']:
                            indent += char
                        else:
                            break
                    new_content = content[:line_end] + '\n' + indent + new_entry + content[line_end:]
                else:
                    new_content = content[:pos] + ' ' + new_entry + content[pos:]
            else:
                # No existing entries, add as the first one (this shouldn't happen in practice)
                pos = languages_start + len('Languages{')
                new_content = content[:pos] + new_entry + content[pos:]
        
        with open(struct_locale_file, 'w', encoding='utf-8') as f:
            f.write(new_content)
        
        print(f"Updated {struct_locale_file}")
    
    def _update_data_category(self, content: str, category: str, locale_code: str) -> str:
        """Update a specific data category in the xcfilelist file."""
        # Find all existing entries for this category
        category_pattern = rf'\$\(SRCROOT\)/data/{category}/([a-zA-Z_]+)\.txt'
        matches = list(re.finditer(category_pattern, content))
        
        if not matches:
            # Category not found, skip
            return content
        
        # Find the correct insertion point alphabetically
        insertion_point = None
        for match in matches:
            existing_code = match.group(1)
            if existing_code > locale_code:
                insertion_point = match
                break
        
        new_entry = f"$(SRCROOT)/data/{category}/{locale_code}.txt\n"
        
        if insertion_point:
            # Insert before this match (at the beginning of the line)
            line_start = content.rfind('\n', 0, insertion_point.start()) + 1
            pos = line_start
            new_content = content[:pos] + new_entry + content[pos:]
        else:
            # Insert at the end of this category's section
            # Find the last entry for this category
            if matches:
                last_match = matches[-1]
                line_end = content.find('\n', last_match.end()) + 1
                new_content = content[:line_end] + new_entry + content[line_end:]
            else:
                # No existing entries found, just append
                new_content = content + new_entry
        
        return new_content
    
    def update_test_files(self, locale_code: str, locale_data: Dict) -> None:
        """Update test files to include the new locale."""
        # This is a simplified version - you may need to add more test file updates
        test_files = [
            self.icu_source / "test" / "cintltst" / "cldrtest.c",
            self.icu_source / "test" / "intltest" / "dtptngts.cpp",
            self.icu_source / "test" / "intltest" / "loctest.cpp"
        ]
        
        for test_file in test_files:
            if test_file.exists():
                print(f"Test file update needed for {test_file} (manual review required)")
    
    def update_build_config(self, locale_code: str) -> None:
        """Update build configuration files."""
        build_file = self.project_root / "icu" / "tools" / "cldr" / "cldr-to-icu" / "build-icu-data.xml"
        
        if build_file.exists():
            print(f"Build config update needed for {build_file} (manual review required)")
    
    def add_locale(self, json_file: Path) -> None:
        """Main method to add a new locale based on JSON input."""
        # Load locale data
        locale_data = self.load_locale_data(json_file)
        locale_code = locale_data["iso639"]
        
        print(f"Adding locale: {locale_code} ({locale_data['names']['en']})")
        
        # Create the main CLDR XML file
        self.create_cldr_xml(locale_code, locale_data)
        
        # Update language names in all existing CLDR files
        self.update_language_names(locale_code, locale_data)
        
        # Update ICU source files
        self.update_uloc_cpp(locale_code, locale_data)
        
        # Update build configuration
        self.update_input_files_xcfilelist(locale_code)
        
        # Update struct locale test file
        self.update_struct_locale_txt(locale_code)
        
        # Update test files (basic notification)
        self.update_test_files(locale_code, locale_data)
        self.update_build_config(locale_code)
        
        print(f"\nLocale {locale_code} has been added successfully!")
        print("Note: Some test files and build configurations may require manual review.")


def main():
    if len(sys.argv) != 2:
        print("Usage: python add_locale.py <json_file>")
        sys.exit(1)
    
    json_file = Path(sys.argv[1])
    if not json_file.exists():
        print(f"Error: JSON file {json_file} not found")
        sys.exit(1)
    
    # Verify we're running from the ICU project root directory
    project_root = Path.cwd()
    required_paths = [
        project_root / "cldr" / "common" / "main",
        project_root / "icu" / "icu4c" / "source" / "common" / "uloc.cpp",
        project_root / "icu" / "icu4c" / "source" / "data" / "inputFiles.xcfilelist",
        project_root / "apple" / "scripts"
    ]
    
    missing_paths = [path for path in required_paths if not path.exists()]
    if missing_paths:
        print("Error: This script must be run from the ICU repository root directory.")
        print("The following required paths were not found:")
        for path in missing_paths:
            print(f"  {path}")
        print("\nPlease run the script as: ./apple/scripts/add-locale.py <json_file>")
        sys.exit(1)
    
    adder = ICULocaleAdder(project_root)
    adder.add_locale(json_file)


if __name__ == "__main__":
    main()