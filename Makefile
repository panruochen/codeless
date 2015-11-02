.PHONY: all clean

define DEF_TARGET
$1:
	make -C cl $$@
	make -C server $$@
endef

$(eval $(call DEF_TARGET,all))
$(eval $(call DEF_TARGET,clean))
$(eval $(call DEF_TARGET,distclean))

.PHONY: run
run: all
	@chmod +x run-demo
	./run-demo

