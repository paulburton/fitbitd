DIR_LOCAL := $(call local-dir)
DIR_LOCAL_OBJ := $(DIR_OBJ)/fitbitd

STATIC_LIBFITBIT ?= 1

fitbitd_pclibs := \
	libcurl \
	mxml \
	dbus-1

fitbitd_src := \
	base64.c \
	control.c \
	devstate.c \
	fitbitd-utils.c \
	main.c \
	postdata.c \
	prefs.c

fitbitd_cflags := \
	-Ilibfitbit \
	-Ilibant \
	$(shell pkg-config --cflags $(fitbitd_pclibs))

fitbitd_ldflags := \
	$(shell pkg-config --libs $(fitbitd_pclibs)) \
	-lpthread \
	-lrt

fitbitd_objects := $(addprefix $(DIR_LOCAL_OBJ)/,$(patsubst %.c,%.o,$(fitbitd_src)))
fitbitd_target := $(DIR_LOCAL_OBJ)/fitbitd

ifeq ($(STATIC_LIBFITBIT),1)
fitbitd_deps += \
	$(libfitbit_a_target) \
	$(libant_a_target)
fitbitd_ldflags += \
	$(libfitbit_a_target) \
	$(libant_a_target) \
	$(shell pkg-config --libs $(libant_pclibs))
else
fitbitd_deps += \
	$(libfitbit_so_target) \
fitbitd_ldflags += \
	-L$(dir $(libfitbit_so_target)) \
	-L$(dir $(libant_so_target)) \
	-lfitbit \
	-lant
endif

all: $(fitbitd_target)
$(fitbitd_target): $(fitbitd_objects)
	@mkdir -p $(dir $@)
	$(CC) $(fitbitd_cflags) $(CFLAGS) -o "$@" $(fitbitd_objects) $(fitbitd_ldflags)

$(fitbitd_objects): $(DIR_LOCAL)/$$(notdir $$(patsubst %.o,%.c,$$@)) $(fitbitd_deps)
	@mkdir -p $(dir $@)
	$(CC) $(fitbitd_cflags) $(CFLAGS) -o "$@" -c "$<"

clean: clean-fitbitd
.PHONY: clean-fitbitd
clean-fitbitd: objdir:=$(DIR_LOCAL_OBJ)
clean-fitbitd:
	rm -rf $(objdir)

install: install-fitbitd
.PHONY: install-fitbitd
install-fitbitd: $(fitbitd_target)
	install -Dm755 "$(fitbitd_target)" "$(DESTDIR)/bin/fitbitd"
