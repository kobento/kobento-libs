cmake_minimum_required(VERSION 3.20)

# Pointer-routing regression for embedded rc-documents (RcDocumentHost).
add_executable(rcdoc_pointer_test rcdoc_pointer_test.cpp)
target_link_libraries(rcdoc_pointer_test PRIVATE rcskia rccore skia_lib)

# Run from the source tree so it finds tests/fixtures/host.rc + media/nested.rc.
add_test(NAME rcdoc_pointer
         COMMAND rcdoc_pointer_test "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# The camera embed's config string.
add_executable(camera_config_test camera_config_test.cpp)
target_link_libraries(camera_config_test PRIVATE rcplayer)
add_test(NAME camera_config COMMAND camera_config_test)

# Click actions: one click must run the action exactly once (counter fixture).
add_executable(click_action_test click_action_test.cpp)
target_link_libraries(click_action_test PRIVATE rcskia rccore skia_lib)
add_test(NAME click_action
         COMMAND click_action_test "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# Host actions and named variables as a host sees them: a hostNamedAction whose name is written
# in its click list arrives with that name; a host finds an exported float by name and sets it.
add_executable(host_action_names_test host_action_names_test.cpp)
target_link_libraries(host_action_names_test PRIVATE rcskia rccore skia_lib)
add_test(NAME host_action_names
         COMMAND host_action_names_test "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_executable(text_style_test text_style_test.cpp)
target_link_libraries(text_style_test PRIVATE rcskia rccore skia_lib)
add_test(NAME text_style
         COMMAND text_style_test "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_executable(property_bundle_test property_bundle_test.cpp)
target_link_libraries(property_bundle_test PRIVATE rcskia rccore skia_lib)
add_test(NAME property_bundle
         COMMAND property_bundle_test "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# Diagnostic (not a test): maps click points to the variables they change.
add_executable(click_probe click_probe.cpp)
target_link_libraries(click_probe PRIVATE rcskia rccore skia_lib)

# MODIFIER_VISIBILITY must reach the screen, in both directions.
add_executable(visibility_test visibility_test.cpp)
target_link_libraries(visibility_test PRIVATE rcskia rccore skia_lib)
add_test(NAME visibility
         COMMAND visibility_test "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# A drag moves the touched slider only (with and without a scrolling parent).
add_executable(slider_touch_test slider_touch_test.cpp)
target_link_libraries(slider_touch_test PRIVATE rcskia rccore skia_lib)
add_test(NAME slider_touch
         COMMAND slider_touch_test "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# TextFromFloat formats as the Java reference (fixture written on the JVM).
add_executable(text_from_float_test text_from_float_test.cpp)
target_link_libraries(text_from_float_test PRIVATE rccore)
add_test(NAME text_from_float
         COMMAND text_from_float_test "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/text_from_float_java.tsv")

# Layout animation: a GONE component whose slot moves is not drawn, and later fades in place.
add_executable(layout_anim_test layout_anim_test.cpp)
target_link_libraries(layout_anim_test PRIVATE rccore)
add_test(NAME layout_anim COMMAND layout_anim_test)

# A text laid out at its own measured width must not wrap (float slack in the wrap checks).
add_executable(text_wrap_test text_wrap_test.cpp)
target_link_libraries(text_wrap_test PRIVATE rcskia rccore skia_lib)
add_test(NAME text_wrap
         COMMAND text_wrap_test "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# A string changed after the load is not reset by later frames or a resize (androidx CL 4445989).
add_executable(root_data_test root_data_test.cpp)
target_link_libraries(root_data_test PRIVATE rcskia rccore skia_lib)
add_test(NAME root_data
         COMMAND root_data_test "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# AddMesh2D FLAG_ANTIALIAS expands as the Java reference (fixture written on the JVM).
add_executable(mesh2d_antialias_test mesh2d_antialias_test.cpp)
target_link_libraries(mesh2d_antialias_test PRIVATE rccore)
add_test(NAME mesh2d_antialias
         COMMAND mesh2d_antialias_test "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/mesh2d_antialias_java.txt")

# BINARY_DATA reads as the Java reference and is stored under its id (fixture from the JVM).
add_executable(binary_data_test binary_data_test.cpp)
target_link_libraries(binary_data_test PRIVATE rccore)
add_test(NAME binary_data
         COMMAND binary_data_test "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/binary_data_java.txt")

# Not a test: prints what this player makes of mesh/blob cases, for the three-way check
# (Java, C++, TypeScript) in rcJson's tools/xcheck.
add_executable(mesh2d_dump mesh2d_dump.cpp)
target_link_libraries(mesh2d_dump PRIVATE rccore)

# LAYOUT_TEXT_INPUT reads as a box and an empty string shows the hint (androidx 7f6f9150bc8).
add_executable(text_input_test text_input_test.cpp)
target_link_libraries(text_input_test PRIVATE rcskia rccore skia_lib)
add_test(NAME text_input
         COMMAND text_input_test "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_executable(scroll_lookup_test scroll_lookup_test.cpp)
target_link_libraries(scroll_lookup_test PRIVATE rcskia rccore skia_lib)
add_test(NAME scroll_lookup
         COMMAND scroll_lookup_test "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
