
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
#cmake_policy(SET CMP0015 NEW)

PROJECT(OApackage)

# ----------------------------------------------------------------------------
#   External packages
# ----------------------------------------------------------------------------

if (0)
  find_package(Boost QUIET COMPONENTS  )
  message(STATUS "Boost_LIBRARIES: ${Boost_LIBRARIES}")
  if (Boost_FOUND)
    message(STATUS "Boost was found")
  endif()
endif()

find_package(SWIG)	# Needed for Python modules
find_package(NumPy QUIET)	# Needed for Python modules
if (NumPy_FOUND)
  message(STATUS "NumPy was found")
endif()

#find_package(MPI QUIET)	# Needed for legacy code and OpenMP functionality
#find_package(MPI)	# Needed for legacy code

# ----------------------------------------------------------------------------
#   Code
# ----------------------------------------------------------------------------
#add_definitions(-DEIGEN_MPL2_ONLY)

include_directories(.)
include_directories(src)
link_directories(${CMAKE_SOURCE_DIR})

if (NOT EXISTS "${CMAKE_SOURCE_DIR}/src/oadevelop.h") 
  set(OADEV 0 CACHE BOOL "Enable development code")
else()
  set(OADEV 1 CACHE BOOL "Enable development code")
endif()

if (NOT EXISTS "${CMAKE_SOURCE_DIR}/src/extend.h") 
  set(OAEXTEND 0 CACHE BOOL "Enable OA generation code")
else()
  set(OAEXTEND 1 CACHE BOOL "Enable OA generation code")
endif()


message(STATUS "OA development code: ${OADEV}")
message(STATUS "OA extend code: ${OAEXTEND}")

set(headers src/pareto.h src/analysis.h src/anyoption.h src/arrayproperties.h src/arraytools.h  src/mathtools.h src/oaoptions.h src/tools.h)
set(srcs src/pareto.cpp src/anyoption.cpp src/md5.cpp src/strength.cpp src/arrayproperties.cpp src/arraytools.cpp src/mathtools.cpp src/tools.cpp src/oaoptions.cpp bitarray/bit_array.cpp)

set(headersextend ${headers} src/extend.h src/strength.h src/lmc.h src/nonroot.h )
set(srcsextend ${srcs} src/extend.cpp src/lmc.cpp src/nonroot.cpp )

set(headersextend ${headersextend} src/InfInt.h)
set(srcsextend ${srcsextend} src/InfInt.cpp)

if(OADEV)
  set(headersextend ${headersextend} src/oadevelop.h)
  set(srcsextend ${srcsextend} src/oadevelop.cpp)
endif()

set(OUTPUTDIR ${CMAKE_SOURCE_DIR})

if(OADEV)
  # if development code, try to use openmp
find_package(OpenMP )

if(OPENMP_FOUND AND OADEV)
   message(STATUS "OpenMP found: OpenMP_C_FLAGS ${OpenMP_C_FLAGS}, OpenMP_EXE_LINKER_FLAGS ${OpenMP_EXE_LINKER_FLAGS}")
     set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
     set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
else()
   message(STATUS "OpenMP not found! building code without OpenMP support")
endif()
endif(OADEV)

# for ccache
if(NOT WIN32 AND 0)
  if("$ENV{USER}" STREQUAL "eendebakpt")
    message(STATUS "Using ccache for compilation")
    set(CMAKE_CXX_COMPILER "ccache")
    set(CMAKE_CXX_COMPILER_ARG1 "g++")
  endif()
endif()

# used for HPC at Antwerpen
option(INTEL_CC "Use the Intel compiler." OFF) 

if (INTEL_CC) 
  find_program(CMAKE_C_COMPILER NAMES icc) 
  find_program(CMAKE_CXX_COMPILER NAMES icpc) 
  find_program(CMAKE_AR NAMES xiar) 
  find_program(CMAKE_LINKER NAMES xild) 

  if (CMAKE_C_COMPILER MATCHES CMAKE_C_COMPILER-NOTFOUND OR 
      CMAKE_CXX_COMPILER MATCHES CMAKE_CXX_COMPILER-NOTFOUND OR 
      CMAKE_AR MATCHES CMAKE_AR-NOTFOUND OR 
      CMAKE_LINKER MATCHES CMAKE_LINKER-NOTFOUND) 
    message(FATAL_ERROR "Cannot find Intel compiler.  You may need to run `.  /opt/intel/bin/compilervars.sh intel64'") 
  endif () 

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMPICH_IGNORE_CXX_SEEK -axSSE4.2,SSE4.1 -O3")

endif (INTEL_CC) 

set(LINKOPTS "")
#message(STATUS "ENV{USERNAME} $ENV{USERNAME}, USER $ENV{USER}")
if( "$ENV{USER}" STREQUAL "vsc20149")
  set(LINKOPTS "-s")	# strip executables for file size
endif()

if (CMAKE_COMPILER_IS_GNUCXX)
  #message(STATUS "CMAKE_COMPILER_IS_GNUCXX ${CMAKE_COMPILER_IS_GNUCXX}")
  #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wreturn-type -Wno-unused-but-set-variable -Wno-unused-result -fPIC -g -fpermissive")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wreturn-type -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-result -fPIC  -fpermissive -g")
  set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}  -O0 -g")
  set(COPTS " -fpermissive -O3 -g")
endif ()

# for debugging
set(COPTS "-g -Os" ) # -std=c++0x ")
set(COPTS "-g -O2 -march=native" ) # -std=c++0x ") # -O3 does not always give the fastest results for executables!
#set(COPTS "-g -O3 -msse2" ) # -std=c++0x ") # -O3 does not always give the fastest results for executables!

#message(STATUS "Setting debug compilation on!")
#set(COPTS "-g -O0 ") # -std=c++0x

if(WIN32)
  set(COPTS "/MT /wd4018 /wd4996")  
endif()
if (MSVC)
  message(STATUS "MSCV compiler")
  set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  /EHsc /W0 /wd4996 /D_SCL_SECURE_NO_WARNINGS")
  ADD_DEFINITIONS(-DNODEFAULTLIB:library)
  set ( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
  #set ( CMAKE_CXX_FLAGS_RELEASE  "${CMAKE_CXX_FLAGS_RELEASE} /NODEFAULTLIB:MSVCRT /EHsc /W0 /MT /verbose" )
 # SET( CMAKE_SHARED_LINKER_FLAGS ${CMAKE_STATIC_LINKER_FLAGS} "/NODEFAULTLIBS:LIBCMT" )
 #  SET( CMAKE_STATIC_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS} "/NODEFAULTLIBS:LIBCMT" )
endif()

if (VERBOSE)
	message(STATUS "copts ${COPTS}")
endif()

if(Boost_FOUND)
  INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )
  set(COPTS "${COPTS} -DHAVE_BOOST")
endif()  

if(OADEV)
  set(COPTS "${COPTS} -DOADEV")
endif()

set(POG 0)	# Profile Guided Optimization

#set(COPTS "${COPTS} -ffast-math")

if(POG STREQUAL "1")
  message(STATUS "POG: profile")
  message(STATUS "POG: run from command line: ")
  set(COPTS "${COPTS} -fprofile-generate --coverage")
  set(LINKOPTS "${LINKOPTS} -fprofile-generate --coverage")
endif()
if(POG STREQUAL "2")
  message(STATUS "POG: use")
  set(COPTS "${COPTS} -fprofile-use")
  set(LINKOPTS "${LINKOPTS} -fprofile-use")
endif()


macro(add_compile_flags _target)
  # Provide user feedback
#  message(STATUS "  Adding compile flags to: ${_target}")
  set_target_properties(${_target} PROPERTIES COMPILE_DEFINITIONS "OAEXTEND_SINGLECORE;NEWINTERFACE " )
  set_target_properties(${_target} PROPERTIES COMPILE_FLAGS "${COPTS}" )
  set_target_properties(${_target} PROPERTIES LINK_FLAGS "${LINKOPTS}")

endmacro()


############### Programs ################################
  
  
set(progs oacat oajoin oapareto oasplit oaanalyse oainfo oaconvert oafilter pareto_example )
set(progsextend )

if(OAEXTEND)
  list(APPEND progs oacheck oastreaming )
endif()
if(OADEV)
  list(APPEND progs oatest oaunittest oa_depth_extend oaextendA oareduceDOP oaclustergather)
endif()
   
if (WIN32)
  add_library(oalib STATIC ${srcsextend} ${headersextend})
  add_compile_flags(oalib)
  add_library(oalibshared SHARED ${srcsextend} ${headersextend})
  add_compile_flags(oalibshared)
else()
  add_library(oalib STATIC ${srcsextend} ${headersextend})
  add_compile_flags(oalib)
  add_library(oalibshared SHARED ${srcsextend} ${headersextend})
  add_compile_flags(oalib)
endif()

if (OAEXTEND)
  add_executable(oaextendsingle "utils/oaextend.cpp" ${headersextend})
  add_compile_flags(oaextendsingle)

  # get_directory_property( DirDefs COMPILE_DEFINITIONS ) 
  #message( "COMPILE_DEFINITIONS = ${DirDefs}" )
  
  TARGET_LINK_LIBRARIES(oaextendsingle oalib)
  set(extendprogs oaextendsingle )


  if (MPI_FOUND AND NOT WIN32 AND 1)
    message(STATUS "Found MPI package: adding oaextendmpi")
    if (VERBOSE)
	message(STATUS "MPI executable: ${MPIEXEC}, libs ${MPI_LIBRARIES}")
    endif()
    #message(STATUS "   libs ${MPI_LIBRARIES}; flags ${MPI_COMPILE_FLAGS}; libs ${MPI_LIBRARIES}")
    include_directories( ${MPI_INCLUDE_PATH} )

  add_executable(oaextendmpi EXCLUDE_FROM_ALL "utils/oaextend.cpp" ${srcs} "src/mpitools.cpp")
  target_link_libraries(oaextendmpi ${MPI_LIBRARIES})
  if(MPI_COMPILE_FLAGS)
    set_target_properties(oaextendmpi PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
  endif()
  if(MPI_LINK_FLAGS)
      if (WIN32)
		set_target_properties(oaextendmpi PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS} /MT")
		endif()
    set_target_properties(oaextendmpi PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")
  endif()
  #set_target_properties(oaextendmpi PROPERTIES COMPILE_FLAGS -I${MPIPATH})
  #TARGET_LINK_LIBRARIES(oaextendmpi /usr/lib/mpich2/lib/libmpichcxx)
  set_target_properties(oaextendmpi PROPERTIES COMPILE_DEFINITIONS "OAEXTEND_MULTICORE;NEWINTERFACE" )

#  list(APPEND extendprogs oaextendmpi) 
   if (WIN32) 
	  else()
	TARGET_LINK_LIBRARIES(oaextendmpi m z)
  endif()
  
endif(MPI_FOUND AND NOT WIN32 AND 1)

message(STATUS "Extend progs: ${extendprogs}")
endif()

message(STATUS "Progs: ${progs}")

if (0)
  # testing
ADD_EXECUTABLE(oacat2 "oacat.cpp" ${headers} ${srcs} ) # ${srcs}
add_compile_flags(oacat2)
TARGET_LINK_LIBRARIES(oacat2 m z)
  target_link_libraries(oacat2 ${Boost_LIBRARIES})
endif()

foreach(item ${progs})
  ADD_EXECUTABLE(${item} "utils/${item}.cpp" ${headers}  ) # ${srcs}
  add_compile_flags(${item})
  TARGET_LINK_LIBRARIES(${item} oalib)

  if (WIN32) 
   #	message(STATUS "Win32 build: ${item}")
  else()
	TARGET_LINK_LIBRARIES(${item} m z)
  endif()
endforeach()

foreach(item ${extendprogs})
  if (WIN32) 
#	message(STATUS "Win32 build: ${item}")
	  else()
	    set_target_properties(${item} PROPERTIES COMPILE_FLAGS "${COPTS}" )
	TARGET_LINK_LIBRARIES(${item} m z)
  endif()
	    set_target_properties(${item} PROPERTIES LINK_FLAGS "${LINKOPTS}" )
endforeach()

if(Boost_FOUND)
  foreach(item ${progs} ${extendprogs})
  target_link_libraries(${item} ${Boost_LIBRARIES})
endforeach()
endif()

## special cases
if(OPENMP_FOUND)
#   message(STATUS "OpenMP found: OpenMP_C_FLAGS ${OpenMP_C_FLAGS}, OpenMP_EXE_LINKER_FLAGS ${OpenMP_EXE_LINKER_FLAGS}")

 #  set_target_properties(oaclustergather PROPERTIES COMPILE_FLAGS "${COPTS} ${OpenMP_CXX_FLAGS}" )
 #  set_target_properties(oaclustergather PROPERTIES LINK_FLAGS "${LINKOPTS} ${OpenMP_EXE_LINKER_FLAGS}" )

   #  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
   #  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
  #   set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
else()
endif()


#include(PrecompiledHeader.cmake)
#add_precompiled_header(oapareto src/pareto.h src/mathtools.h)
#add_precompiled_header(oapareto src/mathtools.h)

####### Python support ##########################

add_custom_target( swig )

if(WIN32)
 add_custom_command(
	TARGET swig PRE_BUILD 
	COMMAND swig -c++  -DOADEV -w503,401,362 -python -Isrc/ -o oalib_wrap_dev.cxx oalib.i
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
  else()
 add_custom_command(
	TARGET swig PRE_BUILD 
	COMMAND swig -c++ -DWIN32 -D_WIN32 -DOADEV -w503,401,362 -python -Isrc/ -o oalib_wrap_dev.cxx oalib.i
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
  endif()

if (WIN32)
  set(DOPYTHON 0)
  message(STATUS "Platform Windows: not building Python module")
  message(STATUS "  see the documentation for details")
else()
  if(SWIG_FOUND)
  set(DOPYTHON 1)
else()
  message(STATUS "SWIG not found: not building Python module")
endif()
endif()

add_custom_target( R )
add_custom_target( R2 )

add_custom_command(
  TARGET R POST_BUILD
  COMMAND swig -c++ -DOADEV -Isrc/ -o oalib_wrapR.cpp -r oalib.i\; export PKG_CXXFLAGS="-Isrc/ -I./ -DSWIGR"\; echo "pre"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  DEPENDS 
)

add_custom_command(
  TARGET R2 POST_BUILD
  COMMAND rm -f oalibR.so\; R CMD SHLIB  -o oalibR.so oalib_wrapR.cpp src/pareto.cpp src/anyoption.cpp src/md5.cpp src/strength.cpp src/arrayproperties.cpp src/arraytools.cpp src/mathtools.cpp src/tools.cpp src/oaoptions.cpp bitarray/bit_array.cpp src/extend.cpp src/oadevelop.cpp src/lmc.cpp src/nonroot.cpp
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  DEPENDS 
)

#export PKG_CXXFLAGS="-Isrc/ -I./ -DSWIGR -DOADEV"
#rm -f oalibR.so; R CMD SHLIB  -o oalibR.so oalib_wrapR.cpp build/liboalibshared.so -d


#rm -f oalibR.so; R CMD SHLIB  -o oalibR.so oalib_wrapR.cpp src/pareto.cpp src/anyoption.cpp src/md5.cpp src/strength.cpp src/arrayproperties.cpp src/arraytools.cpp src/mathtools.cpp src/tools.cpp src/oaoptions.cpp bitarray/bit_array.cpp src/extend.cpp src/oadevelop.cpp src/lmc.cpp src/nonroot.cpp
#nm -C oalibR.so

#build/lib.linux-x86_64-2.7/_oalib.so

#R CMD SHLIB  -o example.so oalib_wrapR.cpp liboalib.a-d
#Rscript example.r




if (DOPYTHON) 
	  message(STATUS "Creating python interface")
	  
add_custom_target( python )
add_custom_target( python3 )	# target for python 3 installation
add_custom_target( pythonswig )
if(OADEV)
  message(STATUS "Building python module with development code")
# --inplace
add_custom_command(
  TARGET python POST_BUILD
  COMMAND swig -c++ -w503,401,362 -python -Isrc/ oalib.i\; swig -c++ -DOADEV  -w503,401,362 -python -o oalib_wrap_dev.cxx -Isrc/ oalib.i\; python setup.py build_ext 
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  DEPENDS pythonswig
)

add_custom_command(
  TARGET python3 POST_BUILD
  COMMAND swig -c++ -DOADEV  -w503,401,362 -python -py3 -o oalib_wrap_dev.cxx -Isrc/ oalib.i\; python3 setup.py build_ext 
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  DEPENDS 
)
else()
add_custom_command(
  TARGET python POST_BUILD
  COMMAND swig   -c++ -w503,401,362 -python -Isrc/ oalib.i\; python setup.py build_ext 
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_custom_command(
  TARGET python3 POST_BUILD
  COMMAND swig -c++  -w503,401,362 -python -Isrc/ oalib.i\; python3 setup.py build_ext 
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
endif()

#set(OPTS -fPIC -DNEWINTERFACE -Wno-return-type -Wno-unused-variable -Wno-unused-result)
if (0)
set(PSRC src/tools.cpp src/arrayproperties.cpp src/mathtools.cpp src/arraytools.cpp src/strength.cpp src/lmc.cpp bitarray/bit_array.cpp)
set(PSRC ${srcs})
set(POBJ tools.o arrayproperties.o oaoptions.o bit_array.o mathtools.o arraytools.o extend.o strength.o lmc.o)

add_custom_target( python2 )
add_custom_command(
  TARGET python2 POST_BUILD
  COMMAND swig -c++ -w503,401,362 -python -I./ -Isrc/ oalib.i\; g++ -I./ -Isrc/ -c ${OPTS} ${PSRC} oalib_wrap.cxx -I/usr/include/python2.7\; g++ -shared ${OPTS} ${POBJ} oalib_wrap.o -o _oalib.so
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_custom_target( python2i )
add_custom_command(
  TARGET python2i POST_BUILD
  COMMAND 	sudo cp _oalib.so /usr/local/lib/python2.7/dist-packages/
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
endif()


#	swig2.0 -c++ -python oalib.i
#	g++ -c $(OPTS) $(PSRC) oalib_wrap.cxx -I/usr/include/python2.7
#	g++ -shared $(OPTS) $(POBJ) oalib_wrap.o -o _oalib.so 

if (0)
add_custom_target( pythoncheck )
add_custom_command(
  TARGET pythoncheck
  POST_BUILD
  COMMAND python python/example_python_testing.py
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
endif()

#add_dependencies(pythoncheck python)
	
add_custom_target( pythondoc )
add_custom_command(
  TARGET pythondoc POST_BUILD
  COMMAND cd xml; xsltproc ../swig.xsl index.xml > swig_doc.i; cd ..
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)


add_custom_target( pinstall )
add_custom_command(
  TARGET pinstall
  POST_BUILD
  COMMAND python setup.py install --user
#  COMMAND sudo python setup.py install
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_custom_target( pinstall3 )
add_custom_command(
  TARGET pinstall3
  POST_BUILD
  COMMAND python3 setup.py install --user
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

endif(DOPYTHON)



set(SWIGP 0)
if (SWIGP)
  # Build Python using SWIG package
  FIND_PACKAGE(SWIG)
  if(SWIG_FOUND AND NOT WIN32)

  INCLUDE(${SWIG_USE_FILE})

  FIND_PACKAGE(PythonLibs)
  INCLUDE_DIRECTORIES(${PYTHON_INCLUDE_PATH})
  INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

  #-w503,401,362
  #-DNEWINTERFACE
  SET(CMAKE_SWIG_FLAGS "-DNEWINTERFACE")

  SET_SOURCE_FILES_PROPERTIES(oalib.i PROPERTIES CPLUSPLUS ON)
  #SET_SOURCE_FILES_PROPERTIES(oalib.i PROPERTIES SWIG_FLAGS "-includeall")
  SET_SOURCE_FILES_PROPERTIES(oalib.i PROPERTIES SWIG_FLAGS "-w503,401,362")
  SET_SOURCE_FILES_PROPERTIES(oalib.i PROPERTIES SWIG_FLAGS "-DNEWINTERFACE")
  SWIG_ADD_MODULE(oalibx python oalib.i ${PSRCS})
  SWIG_LINK_LIBRARIES(oalibx ${PYTHON_LIBRARIES})

  # see: http://www.cmake.org/pipermail/cmake/2006-November/011823.html

  #install (TARGETS _oalibx DESTINATION bin)
endif(SWIGP)

endif()


add_custom_target( doc )
add_custom_command(
  TARGET doc
  POST_BUILD
  COMMAND doxygen Doxyfile
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

#add_custom_target( complete )
#add_dependencies(complete all python python check)



################ INSTALLATION OPTIONS ###########################

foreach(prog ${progs})
	install (TARGETS ${prog} DESTINATION bin)
endforeach()
foreach(prog ${extendprogs})
	install (TARGETS ${prog} DESTINATION bin)
endforeach()
install (TARGETS oalib DESTINATION lib)


install (FILES README.rst FORMAT.txt LICENSE.txt DESTINATION doc)
install (FILES oaconfig.txt DESTINATION bin)
if(DOPYTHON)
#  install (FILES python/example_python_testing.py DESTINATION python)
  install (FILES oapackage/oahelper.py DESTINATION oapackage)
  install (FILES oapackage/markup.py DESTINATION oapackage)
  install (FILES scripts/example_python_testing.py DESTINATION scripts)
  install (FILES scripts/example_python_testing2.py DESTINATION scripts)
endif()

SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Program to calculate orthogonal arrays")
SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")


SET(CPACK_PACKAGE_VERSION_MAJOR "1")
SET(CPACK_PACKAGE_VERSION_MINOR "9")
SET(CPACK_PACKAGE_VERSION_PATCH "70")

#SET(CPACK_STRIP_FILES "bin/MyExecutable")


#set(CPACK_SOURCE_PACKAGE_FILE_NAME "xxx-${VERSION}" CACHE INTERNAL "tarball basename")
set(CPACK_SOURCE_GENERATOR TGZ)

set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}-source")

set(OADEVPACK 1)
set(CPACK_SOURCE_IGNORE_FILES "tests.txt$;/vsc/;/old/;/Eigen3.2/;/Eigen-devel/;/Eigen3.1/;/buildvs/;a.out$;/TestingCommands;/testing/;tags$;nauty$;_oalib.so$;Makefile$;setup.py~$;ChangeLog~$;CMakeFiles$;TODO$;oaconfig-speedtest.txt$;CMakeCache.txt;cmake_install.cmake;.pyc$;/oacode.kdev4;/OA.kde;.cpp~$;/CMakeLists-develop.txt;/build/;/matlabtools/;/xml/;/pythonscripts/;/miscx/;/performancetest/;/oldx/;/buildwin/;/html/;/testsuite/;/.svn/;${CPACK_SOURCE_IGNORE_FILES}")
set(CPACK_SOURCE_IGNORE_FILES "oatest2.cpp;/build2/;PrecompiledHeader.cmake;/Eigen3.2.1/" ${CPACK_SOURCE_IGNORE_FILES})
set(CPACK_SOURCE_IGNORE_FILES "\\\\.cpp~$" "\\\\.txt~$" ${CPACK_SOURCE_IGNORE_FILES})
set(CPACK_SOURCE_IGNORE_FILES "~$" ${CPACK_SOURCE_IGNORE_FILES})

if (VERBOSE)
  message(STATUS "CPACK_SOURCE_IGNORE_FILES ${CPACK_SOURCE_IGNORE_FILES}")
endif()

set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-extend-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}-source")
message(STATUS "Building package with extend options")
if(OADEVPACK)
  set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-develop-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}-source")
	message(STATUS "Building development package!")
else()
  set(CPACK_SOURCE_IGNORE_FILES "/testdata/;/pythondevelop/;${CPACK_SOURCE_IGNORE_FILES}")
  set(CPACK_SOURCE_IGNORE_FILES "oareduceDOP.cpp;oaextendA.cpp$;oatest.cpp$;oa_depth_extend.cpp$;/src/oadevelop.h$;/src/oadevelop.cpp;${CPACK_SOURCE_IGNORE_FILES}")
  #message(STATUS "CPACK_SOURCE_IGNORE_FILES  ${CPACK_SOURCE_IGNORE_FILES}")
endif(OADEVPACK)
#message(STATUS "CPACK_SOURCE_IGNORE_FILES ${CPACK_SOURCE_IGNORE_FILES}")

# cmake package type: CygwinBinary, DEB, TGZ, ZIP, etc.
if(WIN32)
  SET(CPACK_GENERATOR "ZIP")
else()
  SET(CPACK_GENERATOR "DEB")
  #SET(CPACK_GENERATOR "TGZ")
  SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Pieter Eendebak") #required
endif()
INCLUDE(CPack)

add_custom_target(dist COMMAND ${CMAKE_MAKE_PROGRAM} package_source)


### for VCS
#module load SWIG/2.0.4-ictce-4.0.1-Python-2.7.3
#module load zlib/1.2.5-ictce-4.0.1 
#module load CMake/2.8.0-ictce-3.2.1.015.u1   # negeer de ERROR die je krijgt
#export CC=icc
#export CXX=icpc

# cmake -DCMAKE_INSTALL_PREFIX:PATH=~vsc20149 ..

# or
# module load GCC/4.7.0
# cmake -DCMAKE_INSTALL_PREFIX:PATH=~vsc20149 -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ ..

# mmlsquota
