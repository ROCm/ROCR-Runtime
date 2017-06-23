#
# Source files for Rocr Cmdwriter
#
set ( CmdWriterSrcs gfx8_cmdwriter.cpp )
set ( CmdWriterSrcs ${CmdWriterSrcs} gfx9_cmdwriter.cpp )

#
# Build Cmdwriter as a Static Library object
#
add_library ( ${CMDWRITER_LIB} STATIC ${CmdWriterSrcs} )
