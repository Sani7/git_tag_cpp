cmake_minimum_required(VERSION 3.25)

###################################
###### CUSTOMIZATION POINTS #######
###################################

# If you change the output file, remember to modify the GitHash.hpp header file
# in sync with it!

find_package(Git REQUIRED)

# Commands to read each needed variable
set(variablesToRead "GIT_BRANCH;GIT_SHA1;GIT_SHORTSHA1;GIT_TAGS;GIT_DIRTY")
set(CMD_GIT_BRANCH      ${GIT_EXECUTABLE} branch --show-current)
set(CMD_GIT_SHA1        ${GIT_EXECUTABLE} log -1 --format=%H)
set(CMD_GIT_SHORTSHA1   ${GIT_EXECUTABLE} log -1 --format=%h)
set(CMD_GIT_TAGS        ${GIT_EXECUTABLE} describe --tags --match "v*")
set(CMD_GIT_DIRTY       ${GIT_EXECUTABLE} describe --always --dirty) # we post-process this one

# Generator of the .cpp of the githash library
function(genCppContents outputString)
    set(${outputString}
"#ifdef __cplusplus
    namespace Git_version {
    extern const char * const branch;
    extern const char * const sha1;
    extern const char * const shortSha1;
    extern const char * const tags;
    extern const char * const config;
    extern const bool dirty;
    const char * const branch = \"${GIT_BRANCH}\";
    const char * const sha1 = \"${GIT_SHA1}\";
    const char * const shortSha1 = \"${GIT_SHORTSHA1}\";
    const char * const tags = \"${GIT_TAGS}\";
    const char * const config = \"${CMAKE_BUILD_TYPE}\";
    const bool dirty = ${GIT_DIRTY};
}
#else
    extern const char* const Gv_branch;
    extern const char* const Gv_sha1;
    extern const char* const Gv_shortSha1;
    extern const char* const Gv_tags;
    extern const char* const Gv_config;
    extern const bool        Gv_dirty;
    const char* const Gv_branch = \"${GIT_BRANCH}\";
    const char* const Gv_sha1 = \"${GIT_SHA1}\";
    const char* const Gv_shortSha1 = \"${GIT_SHORTSHA1}\";
    const char* const Gv_tags = \"${GIT_TAGS}\";
    const char* const Gv_config = \"${CMAKE_BUILD_TYPE}\";
    const bool        Gv_dirty = ${GIT_DIRTY};
#endif"
        PARENT_SCOPE
    )
endfunction()

# Cache format (which, if changed, triggers a regeneration of the .cpp)
function(genCache outputString)
    set(${outputString} "${GIT_SHA1}-${GIT_DIRTY}-${GIT_TAGS}" PARENT_SCOPE)
endfunction()

###################################
###### CONFIGURATION POINTS #######
###################################

set(GitHash_OutputDir     "${PROJECT_BINARY_DIR}" CACHE STRING "default directory for the output files")
set(GitHash_CppFilename   "git_tag.cpp"                   CACHE STRING "default name of the output cpp file")
set(GitHash_CacheFilename "git_tag_cache.txt"              CACHE STRING "default name of the output cache file")

##########################################################
### You MUST call SetupGitHash in your CMakeLists.txt! ###
##########################################################

# Set utility names for full paths outputs
get_filename_component(GitHash_AbsoluteOutputDir ${GitHash_OutputDir} ABSOLUTE BASE_DIR "${PROJECT_BINARY_DIR}")
set(GitHash_CppFile "${GitHash_AbsoluteOutputDir}/${GitHash_CppFilename}")
set(GitHash_CacheFile "${GitHash_AbsoluteOutputDir}/${GitHash_CacheFilename}")

# Directory where to actually run the Git Commands.
if (NOT DEFINED GitHash_SourceDir)
    set(GitHash_SourceDir "${CMAKE_SOURCE_DIR}")
endif()

function(SetupGitHash)
    # Run this script when building. Note how we pass all variables we need, since we will not get them automatically
    # and even the CMake source dir might be wrong (if for example the build folder is outside the original path)
    add_custom_target(CheckGitHash COMMAND ${CMAKE_COMMAND}
        -DRUN_UPDATE_GIT_HASH=1
        -DGitHash_OutputDir="${GitHash_AbsoluteOutputDir}"
        -DGitHash_CppFilename="${GitHash_CppFilename}"
        -DGitHash_CacheFilename="${GitHash_CacheFilename}"
        -DGitHash_SourceDir="${GitHash_SourceDir}"
        -P ${_THIS_MODULE_FILE}
        BYPRODUCTS ${GitHash_CppFile}
    )

    # Create library for user
    add_library(git_tag ${GitHash_CppFile})
    target_include_directories(git_tag PUBLIC ${CMAKE_CURRENT_LIST_DIR})
    target_sources(git_tag PUBLIC "${CMAKE_CURRENT_LIST_DIR}/git_version.h")
    add_dependencies(git_tag CheckGitHash)

    # Output library name to the other CMakeLists.txt
    set(GITHASH_LIBRARIES git_tag CACHE STRING "Name of git_tag library" FORCE)

    UpdateGitHash()
endfunction()

######################################
### Rest of implementation details ###
######################################

# Needed for setup for older CMake versions (reads this file's path).
set(_THIS_MODULE_FILE "${CMAKE_CURRENT_LIST_FILE}")

# Reads cache file to a variable
function(ReadGitSha1Cache sha1)
    if (EXISTS ${GitHash_CacheFile})
        file(STRINGS ${GitHash_CacheFile} CONTENT)
        LIST(GET CONTENT 0 tmp)

        set(${sha1} ${tmp} PARENT_SCOPE)
    endif()
endfunction()

# Function called during `make`
function(UpdateGitHash)
    # Make sure our working folder exists.
    if (NOT EXISTS ${GitHash_AbsoluteOutputDir})
        file(MAKE_DIRECTORY ${GitHash_AbsoluteOutputDir})
    endif()

    # Automatically set all variables.
    foreach(c ${variablesToRead})
        execute_process(
            COMMAND ${CMD_${c}}
            WORKING_DIRECTORY "${GitHash_SourceDir}"
            OUTPUT_VARIABLE ${c}
            OUTPUT_STRIP_TRAILING_WHITESPACE)
    endforeach(c)

    # GIT_DIRTY post-processing
    if(${GIT_DIRTY} MATCHES ".*dirty")
        set(GIT_DIRTY "true")
    else()
        set(GIT_DIRTY "false")
    endif()

    if(${GIT_TAGS} MATCHES "fatal*")
        set(GIT_TAGS "UNKNW")
    endif()

    # Generate new contents for the cache.
    genCache(newSha1Cache)

    # Try to read old cache
    ReadGitSha1Cache(oldSha1Cache)
    if (NOT DEFINED oldSha1Cache)
        set(oldSha1Cache "none")
    endif()

    # Only update the GitHash.cpp if the hash has changed. This will
    # prevent us from rebuilding the project more than we need to.
    if (NOT ${newSha1Cache} STREQUAL ${oldSha1Cache} OR NOT EXISTS ${GitHash_CppFile})
        # Set the cache so we can skip rebuilding if nothing changed.
        file(WRITE ${GitHash_CacheFile} ${newSha1Cache})

        # Get the CPP file contents with all variables correctly embedded.
        genCppContents(outputString)

        # Finally output our new library cpp file.
        file(WRITE ${GitHash_CppFile} "${outputString}")
        message(STATUS "Compiling branch ${GIT_BRANCH}, commit ${GIT_SHA1}, tag ${GIT_TAGS} dirty is ${GIT_DIRTY}")
    endif()
endfunction()

# This is used to run this function from an external cmake process.
if (RUN_UPDATE_GIT_HASH)
    UpdateGitHash()
else()
    SetupGitHash()
endif()
