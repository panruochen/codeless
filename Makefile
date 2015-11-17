.PHONY: all clean

COMMON_CFLAGS := -DHAVE_MD5
export COMMON_CFLAGS

define DEF_TARGET
$1:
	+make -C cl $$@
	+make -C server $$@
endef

$(eval $(call DEF_TARGET,all))
$(eval $(call DEF_TARGET,clean))
$(eval $(call DEF_TARGET,distclean))

.PHONY: run pre-pack
run: all
	@chmod +x ./tools/run-demo
	./tools/run-demo

pre-pack:
	@rm -rf testsuites/make-3.81
	@git clean -xfd
