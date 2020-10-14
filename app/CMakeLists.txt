cmake_minimum_required(VERSION 3.4.1)

include_directories(src/main/jni/OpenCL)
link_directories(src/main/jni/OpenCL/lib64)

add_library( openclexample1
             SHARED
             src/main/jni/OpenCLexample1.cpp
             src/main/jni/openCLNR.cpp
             src/main/jni/refNR.cpp
            )

# Searches for a specified prebuilt library and stores the path as a
# variable. Because system libraries are included in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
              log-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )



# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in the
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( openclexample1
                       ${log-lib}
                       android
                       OpenCL
                      -ljnigraphics)
#target_link_libraries()