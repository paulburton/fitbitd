DIR_LOCAL := $(call local-dir)
DIR_LOCAL_OBJ := $(DIR_OBJ)/libfitbitdcontrol

libfitbitdcontrol_pclibs := \
	dbus-glib-1

libfitbitdcontrol_src := \
	fitbitd.c

libfitbitdcontrol_cflags := \
	-fPIC \
	$(shell pkg-config --cflags $(libfitbitdcontrol_pclibs))

libfitbitdcontrol_objects := $(patsubst %.c,%.o,$(libfitbitdcontrol_src))
libfitbitdcontrol_a_target := $(DIR_LOCAL_OBJ)/libfitbitdcontrol.a
libfitbitdcontrol_a_objects := $(addprefix $(DIR_LOCAL_OBJ)/static/,$(libfitbitdcontrol_objects))

$(libfitbitdcontrol_a_target): $(libfitbitdcontrol_a_objects)
	@mkdir -p $(dir $@)
	$(AR) rcs "$@" $(libfitbitdcontrol_a_objects)

$(libfitbitdcontrol_a_objects): $(DIR_LOCAL)/$$(notdir $$(patsubst %.o,%.c,$$@))
	@mkdir -p $(dir $@)
	$(CC) $(libfitbitdcontrol_cflags) $(CFLAGS) -o "$@" -c "$<"

clean: clean-libfitbitdcontrol
.PHONY: clean-libfitbitdcontrol
clean-libfitbitdcontrol: objdir:=$(DIR_LOCAL_OBJ)
clean-libfitbitdcontrol:
	rm -rf $(objdir)

deps: libfitbitdcontrol-pkgconfig-deps
.PHONY: libfitbitdcontrol-pkgconfig-deps
libfitbitdcontrol-pkgconfig-deps:
	@pkg-config dbus-glib-1 || ( echo "dbus-glib-1 not found"; exit 1 )
