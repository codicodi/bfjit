function(add_test_native_command name cmd)
    if(WIN32)
        add_test(NAME ${name} COMMAND
            ${CMAKE_COMMAND} -E chdir $<TARGET_FILE_DIR:bfjit> $ENV{ComSpec} /c
            "$<TARGET_FILE_NAME:bfjit> ${cmd}")
    else()
        add_test(NAME ${name} COMMAND /bin/sh -c "$<TARGET_FILE:bfjit> ${cmd}")
    endif()
endfunction()

function(add_test_all_validate_output name file extected_output)
    set(_expected_output_file ${CMAKE_CURRENT_BINARY_DIR}/${name}-expected-output.txt)
    set(_test_source_file ${CMAKE_CURRENT_SOURCE_DIR}/${file})
    set(_extra_args ${ARGN})
    file(WRITE ${_expected_output_file} "${extected_output}")
    function(_atavo_impl confname)
        set(_actual_output_file ${CMAKE_CURRENT_BINARY_DIR}/${name}-${confname}-output.txt)
        add_test_native_command(${name}-${confname}-run 
            "${_test_source_file} ${ARGN} ${_extra_args} > ${_actual_output_file}")
        add_test(NAME ${name}-${confname}-validate COMMAND
            ${CMAKE_COMMAND} -E compare_files ${_actual_output_file} ${_expected_output_file})
        set_tests_properties(${name}-${confname}-validate PROPERTIES DEPENDS ${name}-${confname}-run)
    endfunction()
    _atavo_impl(opt)
    _atavo_impl(dbg --debug)
    _atavo_impl(unsafe --unsafe)
endfunction()

add_test_all_validate_output(cell-size cell-size.b "8 bit cells\n")
add_test_all_validate_output(hello-world hello-world.b "hello world")
add_test_all_validate_output(bitwidth bitwidth.b "Hello World! 255\n")
add_test_all_validate_output(cells30k cells30k.b "OK\n")
add_test_all_validate_output(cells30k-30k cells30k.b "OK\n" "--tape-size 30000")
add_test_all_validate_output(cells30k-50k cells30k.b "OK\n" "--tape-size 50000")

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/mandelbrot-output.txt mandelbrot_output)
add_test_all_validate_output(mandelbrot mandelbrot.b ${mandelbrot_output})

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/long-output.txt long_output)
add_test_all_validate_output(long long.b ${long_output})

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/hanoi-output.txt hanoi_output)
add_test_all_validate_output(hanoi hanoi.b ${hanoi_output})

set(life_input ${CMAKE_CURRENT_BINARY_DIR}/life-input.txt)
file(WRITE ${life_input} "cc\n\nq\n")
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/life-output.txt life_output)
add_test_all_validate_output(life life.b "${life_output}" "< ${life_input}")

set(factor_input ${CMAKE_CURRENT_BINARY_DIR}/factor-input.txt)
file(WRITE ${factor_input} "43564138724\n")
add_test_all_validate_output(factor factor.b "43564138724: 2 2 23 307 1542421\n" "< ${factor_input}")

set(lost_kingdom_input ${CMAKE_CURRENT_BINARY_DIR}/lost-kingdom-input.txt)
file(WRITE ${lost_kingdom_input} "y\nq\ny\nn\n")
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/lost-kingdom-output.txt lost_kingdom_output)
add_test_all_validate_output(lost-kingdom lost-kingdom.b "${lost_kingdom_output}" "< ${lost_kingdom_input}")

set(eof_input ${CMAKE_CURRENT_BINARY_DIR}/eof_input.txt)
file(WRITE ${eof_input} "\n")
add_test_all_validate_output(eof-default  eof.b "LB\nLB\n" "               < ${eof_input}")
add_test_all_validate_output(eof-zero     eof.b "LB\nLB\n" "--eof 0        < ${eof_input}")
add_test_all_validate_output(eof-minusone eof.b "LA\nLA\n" "--eof -1       < ${eof_input}")
add_test_all_validate_output(eof-nochange eof.b "LK\nLK\n" "--eof nochange < ${eof_input}")

function(add_test_fail_impl name file confname msg)
    add_test(NAME ${name}-${confname} COMMAND $<TARGET_FILE:bfjit> ${CMAKE_CURRENT_SOURCE_DIR}/${file} ${ARGN})
    set_tests_properties(${name}-${confname} PROPERTIES WILL_FAIL ON)
    add_test(NAME ${name}-${confname}-msg COMMAND $<TARGET_FILE:bfjit> ${CMAKE_CURRENT_SOURCE_DIR}/${file} ${ARGN})
    set_tests_properties(${name}-${confname}-msg PROPERTIES PASS_REGULAR_EXPRESSION ${msg})
endfunction()

function(add_test_checked_fail name file msg)
    add_test_fail_impl(${name} ${file} opt ${msg} ${ARGN})
    add_test_fail_impl(${name} ${file} dbg ${msg} --debug  ${ARGN})
endfunction()

function(add_test_all_fail name file msg)
    add_test_checked_fail(${name} ${file} ${msg} ${ARGN})
    add_test_fail_impl(${name} ${file} unsafe ${msg} --unsafe ${ARGN})
endfunction()

add_test_all_fail(unmatched-brackets-1 unmatched-brackets-1.b "without a matching")
add_test_all_fail(unmatched-brackets-2 unmatched-brackets-2.b "without a matching")
add_test_all_fail(unmatched-brackets-3 unmatched-brackets-3.b "without a matching")

add_test_checked_fail(out-of-bounds-1 out-of-bounds-1.b "out of bounds")
add_test_checked_fail(out-of-bounds-2 out-of-bounds-2.b "out of bounds")
add_test_checked_fail(out-of-bounds-3 out-of-bounds-3.b "out of bounds")
add_test_checked_fail(out-of-bounds-4 out-of-bounds-4.b "out of bounds")

add_test_checked_fail(out-of-bounds-cells30k cells30k.b "out of bounds" --tape-size 29999)
