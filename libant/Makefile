DIR_LOCAL := $(call local-dir)
DIR_LOCAL_OBJ := $(DIR_OBJ)/libant

libant_pclibs := \
	libusb-1.0

libant_src := \
	ant.c \
	ant-message.c \
	ant-usb.c \
	ant-usb-fitbit.c

libant_cflags := \
	$(shell pkg-config --cflags $(libant_pclibs))

libant_cflags_static := \
	$(libant_cflags)

libant_cflags_shared := \
	$(libant_cflags) \
	-fPIC

libant_ldflags := \
	$(shell pkg-config --libs $(libant_pclibs))

libant_objects := $(patsubst %.c,%.o,$(libant_src))
libant_a_target := $(DIR_LOCAL_OBJ)/libant.a
libant_a_objects := $(addprefix $(DIR_LOCAL_OBJ)/static/,$(libant_objects))
libant_so_target := $(DIR_LOCAL_OBJ)/libant.so
libant_so_objects := $(addprefix $(DIR_LOCAL_OBJ)/shared/,$(libant_objects))

$(libant_a_target): $(libant_a_objects)
	@mkdir -p $(dir $@)
	$(AR) rcs "$@" $(libant_a_objects)

$(libant_a_objects): $(DIR_LOCAL)/$$(notdir $$(patsubst %.o,%.c,$$@))
	@mkdir -p $(dir $@)
	$(CC) $(libant_cflags_static) $(CFLAGS) -o "$@" -c "$<"

$(libant_so_target): $(libant_so_objects)
	@mkdir -p $(dir $@)
	$(CC) -shared $(libant_cflags_shared) $(CFLAGS) $(libant_ldflags) -o "$@" $(libant_so_objects)

$(libant_so_objects): $(DIR_LOCAL)/$$(notdir $$(patsubst %.o,%.c,$$@))
	@mkdir -p $(dir $@)
	$(CC) $(libant_cflags_shared) $(CFLAGS) -o "$@" -c "$<"

clean: clean-libant
.PHONY: clean-libant
clean-libant: objdir:=$(DIR_LOCAL_OBJ)
clean-libant:
	rm -rf $(objdir)

deps: libant-pkgconfig-deps
.PHONY: libant-pkgconfig-deps
libant-pkgconfig-deps:
	@pkg-config libusb-1.0 || ( echo "libusb-1.0 not found"; exit 1 )
