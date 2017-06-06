#
# Source files for Rocr Cmdwriter
#
set ( CmdWriterSrcs gfx8_cmdwriter.cpp )
set ( CmdWriterSrcs ${CmdWriterSrcs} gfx9_cmdwriter.cpp )

#
# Header files include path(s).
#
include_directories ( $ENV{ROCR_INC_DIR} )

#
# Build Cmdwriter as a Static Library object
#
add_library ( ${CMDWRITER_LIB} STATIC ${CmdWriterSrcs} )
