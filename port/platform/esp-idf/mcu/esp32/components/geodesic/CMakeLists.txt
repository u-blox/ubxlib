message("Processing GEODESIC component starts")

# Add GeographicLib as an ESP-IDF component
get_filename_component(UBXLIB_BASE ${CMAKE_CURRENT_LIST_DIR}/../../../../../../../ ABSOLUTE)

set(GEODESIC_COMPONENT_DIR ${UBXLIB_BASE}/common/geofence/geographiclib)
file(GLOB COMPONENT_SRCS ${GEODESIC_COMPONENT_DIR}/src/*.cpp)
set(GEODESIC_COMPONENT_INC ${GEODESIC_COMPONENT_DIR}/include
                           ${GEODESIC_COMPONENT_DIR}/include/GeographicLib)
set(COMPONENT_ADD_INCLUDEDIRS ${GEODESIC_COMPONENT_INC})

set(GEOGRAPHICLIB_PRECISION 2)
configure_file (
 ${GEODESIC_COMPONENT_DIR}/include/GeographicLib/Config.h.in
 ${GEODESIC_COMPONENT_DIR}/include/GeographicLib/Config.h
 @ONLY)

register_component()

SET_TARGET_PROPERTIES(${COMPONENT_LIB} PROPERTIES CXX_STANDARD 11)
target_compile_options(${COMPONENT_LIB} PRIVATE -fexceptions)
# These definitions, being public, are meant to be propagated to
# any component that relies on this, e.g. ubxlib.  However,
# for reasons I don't understand, they don't, so will need to
# be duplicated there
target_compile_definitions(${COMPONENT_LIB} PUBLIC GEOGRAPHICLIB_SHARED_LIB=0
                           U_CFG_GEOFENCE_USE_GEODESIC)

message("Processing GEODESIC component ends")
