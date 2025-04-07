# Otto Makefile

NAME = Otto
TYPE = APP
APP_MIME_SIG = application/x-vnd.nexus6-otto
TARGET_DIR = .
SRCS = \
	src/Otto.cpp \
	src/MainWindow.cpp \
	src/ChatView.cpp \
	src/ChatMessage.cpp \
	src/LLMModel.cpp \
	src/LLMProvider.cpp \
	src/ModelSelector.cpp \
	src/BFSStorage.cpp \
	src/SettingsManager.cpp \
	src/SettingsView.cpp \
	src/SettingsWindow.cpp \
	src/ModelManager.cpp \
	src/MCPManager.cpp \
	src/MCPTool.cpp \
	src/providers/OpenAIProvider.cpp \
	src/providers/AnthropicProvider.cpp \
	src/providers/OllamaProvider.cpp

RDEFS = \
	src/Otto.rdef

RSRCS =

LIBS = be translation netservices2 bnetapi textencoding localestub shared $(STDCPPLIBS)

LIBPATHS =

SYSTEM_INCLUDE_PATHS =  /boot/system/develop/headers/os \
 /boot/system/develop/headers/c++ \
 /boot/system/develop/headers/posix \
 /boot/system/develop/headers/private/support \
 /boot/system/develop/headers/private/shared \
 /boot/system/develop/headers/private/netservices2

LOCAL_INCLUDE_PATHS = src src/external
OPTIMIZE := NONE
LOCALES =
DEFINES =
WARNINGS =
SYMBOLS :=
DEBUGGER := TRUE
COMPILER_FLAGS = -std=c++20 -gdwarf-3
LINKER_FLAGS =
DRIVER_PATH =

## Include the Makefile-Engine
DEVEL_DIRECTORY := \
	$(shell findpaths -r "makefile_engine" B_FIND_PATH_DEVELOP_DIRECTORY)
include $(DEVEL_DIRECTORY)/etc/makefile-engine
