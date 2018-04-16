#
# Generated Makefile - do not edit!
#
# Edit the Makefile in the project folder instead (../Makefile). Each target
# has a -pre and a -post target defined where you can add customized code.
#
# This makefile implements configuration specific macros and targets.


# Environment
MKDIR=mkdir
CP=cp
GREP=grep
NM=nm
CCADMIN=CCadmin
RANLIB=ranlib
CC=gcc
CCC=g++
CXX=g++
FC=gfortran
AS=as

# Macros
CND_PLATFORM=GNU-Linux
CND_DLIB_EXT=so
CND_CONF=Debug
CND_DISTDIR=dist
CND_BUILDDIR=build

# Include project Makefile
include Makefile

# Object Directory
OBJECTDIR=${CND_BUILDDIR}/${CND_CONF}/${CND_PLATFORM}

# Object Files
OBJECTFILES= \
	${OBJECTDIR}/EncDecSim.o \
	${OBJECTDIR}/LoadKey.o \
	${OBJECTDIR}/USBDevice.o \
	${OBJECTDIR}/USBHasp.o \
	${OBJECTDIR}/USBKeyEmu.o


# C Compiler Flags
CFLAGS=

# CC Compiler Flags
CCFLAGS=
CXXFLAGS=

# Fortran Compiler Flags
FFLAGS=

# Assembler Flags
ASFLAGS=

# Link Libraries and Options
LDLIBSOPTIONS=-L/usr/local/lib -lusb_vhci -ljansson -lpthread

# Build Targets
.build-conf: ${BUILD_SUBPROJECTS}
	"${MAKE}"  -f nbproject/Makefile-${CND_CONF}.mk ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/usbhasp

${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/usbhasp: ${OBJECTFILES}
	${MKDIR} -p ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}
	${LINK.c} -o ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/usbhasp ${OBJECTFILES} ${LDLIBSOPTIONS}

${OBJECTDIR}/EncDecSim.o: EncDecSim.c
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -g -DDEBUG=2 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/EncDecSim.o EncDecSim.c

${OBJECTDIR}/LoadKey.o: LoadKey.c
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -g -DDEBUG=2 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/LoadKey.o LoadKey.c

${OBJECTDIR}/USBDevice.o: USBDevice.c
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -g -DDEBUG=2 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/USBDevice.o USBDevice.c

${OBJECTDIR}/USBHasp.o: USBHasp.c
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -g -DDEBUG=2 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/USBHasp.o USBHasp.c

${OBJECTDIR}/USBKeyEmu.o: USBKeyEmu.c
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -g -DDEBUG=2 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/USBKeyEmu.o USBKeyEmu.c

# Subprojects
.build-subprojects:

# Clean Targets
.clean-conf: ${CLEAN_SUBPROJECTS}
	${RM} -r ${CND_BUILDDIR}/${CND_CONF}

# Subprojects
.clean-subprojects:

# Enable dependency checking
.dep.inc: .depcheck-impl

include .dep.inc
