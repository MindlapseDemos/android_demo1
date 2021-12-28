.PHONY: all
all: imago anim psys treestore

.PHONY: clean
clean: clean-imago clean-anim clean-psys clean-treestore


.PHONY: imago
imago:
	$(MAKE) -C imago

.PHONY: clean-imago
clean-imago:
	$(MAKE) -C imago clean

.PHONY: anim
anim:
	$(MAKE) -C anim

.PHONY: clean-anim
clean-anim:
	$(MAKE) -C anim clean

.PHONY: psys
psys:
	$(MAKE) -C psys

.PHONY: clean-psys
clean-psys:
	$(MAKE) -C psys clean

.PHONY: treestore
treestore:
	$(MAKE) -C treestore

.PHONY: clean-treestore
clean-treestore:
	$(MAKE) -C treestore clean
