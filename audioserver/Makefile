####################################################
# Generic makefile
# for compiling and linking C++ projects on Linux 
####################################################
### Customising
#
# Adjust the following if necessary; EXECUTABLE is the target
# executable's filename, and LIBS is a list of libraries to link in
# (e.g. alleg, stdcx, iostr, etc). You can override these on make's
# command line of course, if you prefer to do it that way.
#
#
EXECUTABLE := AudioRcvServer   # 可执行文件名
LIBDIR:=      # 静态库目录
LIBS :=   pthread dl     # 静态库文件名
INCLUDES:= .\
           ./include/*
          # 头文件目录
SRCDIR:= ./include/*   # 除了当前目录外，其他的源代码文件目录
#
# # Now alter any implicit rules' variables if you like, e.g.:
 
CC:=g++
ifndef RELEASE
CFLAGS := -Wall -DMY_DEBUG -g -O0 -std=c++11
else
CFLAGS := -Wall -DMY_DEBUG -g -O2 -std=c++11
endif
CPPFLAGS := $(CFLAGS)
CPPFLAGS += $(addprefix -I,$(INCLUDES))
CPPFLAGS += -MMD
#
# # The next bit checks to see whether rm is in your djgpp bin
# # directory; if not it uses del instead, but this can cause (harmless)
# # `File not found' error messages. If you are not using DOS at all,
# # set the variable to something which will unquestioningly remove
# # files.
#
 
RM-F := rm -f
 
 
# # You shouldn't need to change anything below this point.
#
SRCS := $(wildcard *.cpp) $(wildcard $(addsuffix /*.cpp, $(SRCDIR)))
OBJS := $(patsubst %.cpp,%.o,$(SRCS))
DEPS := $(patsubst %.o,%.d,$(OBJS))
MISSING_DEPS := $(filter-out $(wildcard $(DEPS)),$(DEPS))
MISSING_DEPS_SOURCES := $(wildcard $(patsubst %.d,%.cpp,$(MISSING_DEPS)))
 
 
.PHONY : all deps objs clean veryclean rebuild info
 
all: $(EXECUTABLE)
 
deps : $(DEPS)
 
objs : $(OBJS)
 
clean :
	@$(RM-F) $(OBJS)
	@$(RM-F) $(DEPS)
veryclean: clean
	@$(RM-F) $(EXECUTABLE)
 
rebuild: veryclean all
ifneq ($(MISSING_DEPS),)
$(MISSING_DEPS) :
	@$(RM-F) $(patsubst %.d,%.o,$@)
endif
-include $(DEPS)
$(EXECUTABLE) : $(OBJS)
	$(CC) -o $(EXECUTABLE) $(OBJS) $(addprefix -L,$(LIBDIR)) $(addprefix -l,$(LIBS))
info:
	@echo $(SRCS)
	@echo $(OBJS)
	@echo $(DEPS)
	@echo $(MISSING_DEPS)
	@echo $(MISSING_DEPS_SOURCES)
