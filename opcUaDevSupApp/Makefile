TOP=..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

DBD = opcUa.dbd

LIBRARY_HOST += opcUa
opcUa_SRCS = devOpcUa.c drvOpcUa.cpp devUaSubscription.cpp
INC += devOpcUa.h drvOpcUa.h

UASDK_LIBS = uabase uaclient uapki uastack xmlparser

USR_INCLUDES += $(foreach module, $(UASDK_LIBS), -I$(UASDK)/include/$(module))

USR_INCLUDES_WIN32 += -I$(BOOST)

ifeq ($(UASDK_DEPLOY_MODE),SYSTEM)
USR_SYS_LIBS += $(UASDK_LIBS)
endif

ifeq ($(UASDK_DEPLOY_MODE),PROVIDED)
define UA_template
  USR_LIBS += $(1)
  $(1)_DIR = $(UASDK_DIR)
endef
$(foreach lib, $(UASDK_LIBS), $(eval $(call UA_template,$(lib))))
endif

ifeq ($(UASDK_DEPLOY_MODE),INSTALL)
EXTLIB_INSTALLS += $(addprefix $(INSTALL_LIB)/, \
    $(notdir $(wildcard $(foreach lib, $(UASDK_LIBS), \
    $(UASDK_DIR)/$(LIB_PREFIX)$(lib)$(LIB_SUFFIX) ))))
    ifeq ($(SHARED_LIBRARIES),YES)
EXTLIB_INSTALLS += $(addprefix $(INSTALL_LIB)/, \
    $(notdir $(wildcard $(foreach lib, $(UASDK_LIBS), \
    $(UASDK_DIR)/$(SHRLIB_PREFIX)$(lib)$(SHRLIB_SUFFIX) ))))
    endif
endif

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

ifeq ($(UASDK_DEPLOY_MODE),INSTALL)
build: $(EXTLIB_INSTALLS)

$(INSTALL_LIB)/$(LIB_PREFIX)%$(LIB_SUFFIX) : $(UASDK_DIR)/$(LIB_PREFIX)%$(LIB_SUFFIX)
	$(ECHO) "Installing library $@ from Unified Automation SDK"
	$(INSTALL) -d -m 444 $< $(@D)

$(INSTALL_LIB)/$(SHRLIB_PREFIX)%$(SHRLIB_SUFFIX) : $(UASDK_DIR)/$(SHRLIB_PREFIX)%$(SHRLIB_SUFFIX)
	$(ECHO) "Installing shared library $@ from Unified Automation SDK"
	$(INSTALL) -d -m 555 $< $(@D)
endif
