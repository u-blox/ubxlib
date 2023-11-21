# Introduction
This directory provides a flexible geofence implementation that runs on this MCU and can be used with GNSS, cellular or short-range devices.

# Usage
The [api](api) directory defines the geofence API; a geofence may consist of multiple circles and/or multiple polygons with multiple vertices.  The geofence(s) created in this way may be applied using the [uGnssGeofence](/gnss/api/u_gnss_geofence.h), [uCellGeofence](/cell/api/u_cell_geofence.h) or [uWifiGeofence](/wifi/api/u_wifi_geofence.h) APIs such that, whenever a GNSS or CellLocate (cellular) or Google Maps/Skyhook/Here (Wi-Fi) location fix is achieved it is tested against the fence.

This code is only included if `U_CFG_GEOFENCE` is defined, since maths and floating point operations are required.  If you wish to create fences that are larger than 1 km in size then you should employ a true earth model, using WGS84 coordinates: see the instructions at the top of [u_geofence_geodesic.h](api/u_geofence_geodesic.h) and the note below about [GeographicLib](https://github.com/geographiclib) for how to do this.

The [test](test) directory contains tests that can be run on mostly on any platform, though note that some larger tests will only run on Windows.

# Sub-module [geographiclib](https://github.com/geographiclib)
If you do not provide your own geodesic functions and intend to use fences with shapes greated than 1 km in size, where the non-spherical nature of the earth has an impact, [GeographicLib](https://github.com/geographiclib) should be used.  To obtain this as a sub-module, make sure that you have done:

`git submodule update --init --recursive`

...and:

- when you set the CMake variable `UBXLIB_FEATURES` in your `CMakeLists.txt` file, add `geodesic` to the list of features,
- define the conditional compilation flag `U_CFG_GNSS_FENCE_USE_GEODESIC` for your build of `ubxlib` (this will happen automatically on some platforms but, unfortunately, it does not on ESP-IDF and there is no harm in adding it anyway, to be completely sure),
- pass the CMake variable `UBXLIB_EXTRA_LIBS`, as set by `ubxlib.cmake`, to the link stage of your `CMakeLists.txt`.

Of course, you must also have defined `U_CFG_GEOFENCE` for your build, in order that geofencing is included at all.