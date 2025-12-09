# Helper function to create examples
function(add_coro_example name)
    add_executable("coro_${name}" ${name}.cpp)
    target_link_libraries("coro_${name}" PRIVATE edge_base edge_coro)
endfunction()

add_coro_example(example_generator)
add_coro_example(example_pipeline)
add_coro_example(example_scheduler)