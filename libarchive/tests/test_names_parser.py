import re

def extract_test_names_from_c_header(c_header):
    c_header = remove_comments_from_c_header(c_header)
    return extract_test_names_from_c_header_without_comments(c_header)

def remove_comments_from_c_header(c_header):
    c_header = remove_line_comments_from_c_header(c_header)
    c_header = remove_block_comments_from_c_header(c_header)
    return c_header

def remove_line_comments_from_c_header(c_header):
    return re.sub('//+.*\n+', '', c_header)

def remove_block_comments_from_c_header(c_header):
    return re.sub('(/\*)(.*?\n*?)*(\*/)', '', c_header)

def extract_test_names_from_c_header_without_comments(c_header):
    return re.findall('DEFINE_TEST+\(+(\w+)+\)', c_header)