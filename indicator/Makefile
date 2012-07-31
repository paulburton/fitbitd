DIR_LOCAL := $(call local-dir)
DIR_LOCAL_OBJ := $(DIR_OBJ)/indicator

INDICATORSDIR ?= $(DESTDIR)/$(patsubst $(shell pkg-config --variable=prefix indicator3-0.4)/%,%,$(shell pkg-config --variable=indicatordir indicator3-0.4))
ICONSDIR_INST ?= $(DESTDIR)/$(patsubst $(shell pkg-config --variable=prefix indicator3-0.4)/%,%,$(shell pkg-config --variable=iconsdir indicator3-0.4))
ICONSDIR_RUN  ?= $(shell pkg-config --variable=iconsdir indicator3-0.4)

indicator_pclibs := \
    indicator3-0.4 \
    gtk+-3.0

indicator_src := \
    indicator.c \
    tracker-menuitem.c

indicator_cflags := \
    -fPIC \
    -Ilibfitbitdcontrol \
    $(shell pkg-config --cflags $(indicator_pclibs)) \
    -DICONS_DIR=\"$(ICONSDIR_RUN)\"

indicator_ldflags := \
    -shared \
    $(libfitbitdcontrol_a_target) \
    $(shell pkg-config --libs $(libfitbitdcontrol_pclibs)) \
    $(shell pkg-config --libs $(indicator_pclibs))

indicator_objects := $(addprefix $(DIR_LOCAL_OBJ)/,$(patsubst %.c,%.o,$(indicator_src)))
indicator_target := $(DIR_LOCAL_OBJ)/libfitbitmenu.so

.PHONY: indicator
indicator: $(indicator_target)
$(indicator_target): $(indicator_objects) $(libfitbitdcontrol_a_target)
	@mkdir -p $(dir $@)
	$(CC) $(indicator_cflags) $(CFLAGS) -o "$@" $(indicator_objects) $(indicator_ldflags)

$(indicator_objects): $(DIR_LOCAL)/$$(notdir $$(patsubst %.o,%.c,$$@))
	@mkdir -p $(dir $@)
	$(CC) $(indicator_cflags) $(CFLAGS) -o "$@" -c "$<"

clean: clean-indicator
.PHONY: clean-indicator
clean-indicator: objdir:=$(DIR_LOCAL_OBJ)
clean-indicator:
	rm -rf $(objdir)

install: install-indicator
.PHONY: install-indicator
install-indicator: ldir:=$(DIR_LOCAL)
install-indicator: idir:=$(ICONSDIR_INST)/hicolor/scalable/status
install-indicator: $(indicator_target)
	install -Dm755 "$(indicator_target)" "$(INDICATORSDIR)/libfitbitmenu.so"
	install -Dm644 "$(ldir)/icons/indicator-fitbit-no-base.svg" "$(idir)/indicator-fitbit-no-base.svg"
	install -Dm644 "$(ldir)/icons/indicator-fitbit-idle.svg" "$(idir)/indicator-fitbit-idle.svg"
	install -Dm644 "$(ldir)/icons/indicator-fitbit-syncing.svg" "$(idir)/indicator-fitbit-syncing.svg"

deps: indicator-pkgconfig-deps
.PHONY: indicator-pkgconfig-deps
indicator-pkgconfig-deps:
	@pkg-config gtk+-3.0 || ( echo "gtk+-3.0 not found"; exit 1 )
	@pkg-config indicator3-0.4 || ( echo "indicator3-0.4 not found"; exit 1 )
