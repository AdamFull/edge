if(NOT DEFINED INPUT_FILE OR NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "INPUT_FILE and OUTPUT_FILE must be defined")
endif()

if(NOT EXISTS ${INPUT_FILE})
    message(FATAL_ERROR "Input file does not exist: ${INPUT_FILE}")
endif()

file(READ ${INPUT_FILE} file_content HEX)
file(SIZE ${INPUT_FILE} file_size)

math(EXPR remainder "${file_size} % 4")
if(NOT remainder EQUAL 0)
    message(WARNING "SPIR-V file size is not a multiple of 4 bytes")
endif()

string(LENGTH ${file_content} hex_length)
math(EXPR num_uint32s "${hex_length} / 8")

set(output_content "")
set(uint32_per_line 4)

foreach(i RANGE 0 ${num_uint32s})
    if(i EQUAL ${num_uint32s})
        break()
    endif()
    
    math(EXPR pos "${i} * 8")
    string(SUBSTRING ${file_content} ${pos} 8 word_hex)
    
    string(SUBSTRING ${word_hex} 0 2 byte0)
    string(SUBSTRING ${word_hex} 2 2 byte1)
    string(SUBSTRING ${word_hex} 4 2 byte2)
    string(SUBSTRING ${word_hex} 6 2 byte3)
    
    set(uint32_value "0x${byte3}${byte2}${byte1}${byte0}")
    
    if(i GREATER 0)
        string(APPEND output_content ", ")
    endif()
    
    math(EXPR line_pos "${i} % ${uint32_per_line}")
    if(i GREATER 0 AND line_pos EQUAL 0)
        string(APPEND output_content "\n")
    endif()
    
    string(APPEND output_content "${uint32_value}")
endforeach()

if(num_uint32s GREATER 0)
    string(APPEND output_content "\n")
endif()

file(WRITE ${OUTPUT_FILE} "${output_content}")