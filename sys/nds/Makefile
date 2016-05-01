#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM)
endif

include $(DEVKITARM)/base_rules

CCMAKE=$(MAKE) CC=cc

#export TARGET		:=	$(shell basename $(CURDIR))
export TARGET		:=	NetHackDS
export TARGETDIR	:=	build
export TOPDIR		:=	$(CURDIR)

#---------------------------------------------------------------------------------
# path to tools - this can be deleted if you set the path in windows
#---------------------------------------------------------------------------------
export PATH		:=	$(DEVKITARM)/bin:$(PATH)

.PHONY: $(TARGET).arm7 $(TARGET).arm9

all: tags dat image

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
dat::
	@cd dat; $(CCMAKE)
	@cd src; $(CCMAKE) ../include/onames.h ../include/pm.h monstr.c ../include/vis_tab.h vis_tab.c tile.c ../include/date.h
	@if egrep -s librarian dat/options ; then $(CCMAKE) dlb ; else true ; fi

image: binary/$(TARGET).ds.gba

tags::
	@if [ -x ./build_idx ]; then ./build_idx; fi

binary/$(TARGET).ds.gba	: build/$(TARGET).ds.gba
	@echo Copying files...
	@[ -d binary ] || mkdir binary
	@rm -rf binary/*
	@cp build/$(TARGET).ds.gba build/$(TARGET).nds binary
	@mkdir binary/NetHack
	@mkdir binary/NetHack/save
	@touch binary/NetHack/record
	@cp dat/nhdat dat/license doc/Guidebook.txt doc/nethack.txt binary/NetHack
	@cp sys/nds/NetHack/* binary/NetHack
	@echo Done!  Copy everything in the 'binary' directory to your CF card.
	@echo Note: If you compiled without DLB defined, you must copy the files listed in
	@echo 'dat/dlb.lst to binary/NetHack.  Try: cd dat; cp `cat dlb.lst` ../binary/NetHack'
	@echo 

install: tags binary/$(TARGET).ds.gba
	cp binary/$(TARGET).nds /media/373A-579A/DEFAULT.NDS
	umount /media/373A-579A

build/$(TARGET).ds.gba	: build/$(TARGET).nds
	@dsbuild $<
	@echo built ... $(notdir $@)

#---------------------------------------------------------------------------------
build/$(TARGET).nds	:	build/$(TARGET).arm7 build/$(TARGET).arm9
	ndstool	-c build/$(TARGET).nds -7 build/$(TARGET).arm7 -9 build/$(TARGET).arm9

#---------------------------------------------------------------------------------
build/$(TARGET).arm7	: build/$(TARGET).arm7.elf
	@$(OBJCOPY) -O binary $< $@
	@echo built ... $(notdir $@)
build/$(TARGET).arm9	: build/$(TARGET).arm9.elf
	@$(OBJCOPY) -O binary $< $@
	@echo built ... $(notdir $@)
#---------------------------------------------------------------------------------
build/$(TARGET).arm7.elf::
	@[ -d build ] || mkdir build
	$(MAKE) -f sys/nds/Makefile.arm7 all
	
#---------------------------------------------------------------------------------
build/$(TARGET).arm9.elf::
	@[ -d build ] || mkdir build
	sh -c "unset CC; [ -f src/tile.c ] || make -C util ../src/tile.c"
	$(MAKE) -f sys/nds/Makefile.src all
	$(MAKE) -f sys/nds/Makefile.arm9 all

#---------------------------------------------------------------------------------
clean:
	$(CCMAKE) spotless
	$(MAKE) -f sys/nds/Makefile.arm7 clean
	$(MAKE) -f sys/nds/Makefile.src clean
	$(MAKE) -f sys/nds/Makefile.arm9 clean
	rm -rf build/*
	rm -rf binary/*
