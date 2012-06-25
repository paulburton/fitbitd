DIR_LOCAL := $(call local-dir)
DIR_LOCAL_OBJ := $(DIR_OBJ)/libfitbit

libfitbit_src := \
	fitbit.c

libfitbit_cflags := \
	-Ilibant

libfitbit_cflags_static := \
	$(libfitbit_cflags)

libfitbit_cflags_shared := \
	$(libfitbit_cflags) \
	-fPIC

libfitbit_ldflags_shared := \
	-L$(dir $(libant_so_target)) \
	-lant

libfitbit_objects := $(patsubst %.c,%.o,$(libfitbit_src))
libfitbit_a_target := $(DIR_LOCAL_OBJ)/libfitbit.a
libfitbit_a_objects := $(addprefix $(DIR_LOCAL_OBJ)/static/,$(libfitbit_objects))
libfitbit_so_target := $(DIR_LOCAL_OBJ)/libfitbit.so
libfitbit_so_objects := $(addprefix $(DIR_LOCAL_OBJ)/shared/,$(libfitbit_objects))

$(libfitbit_a_target): $(libfitbit_a_objects)
	@mkdir -p $(dir $@)
	$(AR) rcs "$@" $(libfitbit_a_objects)

$(libfitbit_a_objects): $(DIR_LOCAL)/$$(notdir $$(patsubst %.o,%.c,$$@))
	@mkdir -p $(dir $@)
	$(CC) $(libfitbit_cflags_static) $(CFLAGS) -o "$@" -c "$<"

$(libfitbit_so_target): $(libfitbit_so_objects)
	@mkdir -p $(dir $@)
	$(CC) -shared $(libfitbit_cflags_shared) $(CFLAGS) $(libfitbit_ldflags_shared) -o "$@" $(libfitbit_so_objects)

$(libfitbit_so_objects): $(DIR_LOCAL)/$$(notdir $$(patsubst %.o,%.c,$$@)) $(libant_so_target)
	@mkdir -p $(dir $@)
	$(CC) $(libfitbit_cflags_shared) $(CFLAGS) -o "$@" -c "$<"

clean: clean-libfitbit
.PHONY: clean-libfitbit
clean-libfitbit: objdir:=$(DIR_LOCAL_OBJ)
clean-libfitbit:
	rm -rf $(objdir)
