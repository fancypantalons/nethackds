#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM)
endif

include $(DEVKITARM)/base_rules

CCMAKE=$(MAKE) CC=cc

#export TARGET		:=	$(shell basename $(CURDIR))
export TARGET		:=	SlashEmDS
export TARGETDIR	:=	build
export TOPDIR		:=	$(CURDIR)



#---------------------------------------------------------------------------------
# path to tools - this can be deleted if you set the path in windows
#---------------------------------------------------------------------------------
export PATH		:=	$(DEVKITARM)/bin:$(PATH)

.PHONY: $(TARGET).arm7 $(TARGET).arm9

all: dat image

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
dat::
	cd dat; $(CCMAKE)
	cd src; $(CCMAKE) ../include/onames.h ../include/pm.h monstr.c ../include/vis_tab.h vis_tab.c tile.c ../include/date.h 
	cd include; ../util/makedefs -f
	@if egrep -s librarian dat/options ; then $(CCMAKE) dlb ; else true ; fi

image: binary/$(TARGET).ds.gba

binary/$(TARGET).ds.gba	: build/$(TARGET).ds.gba
	@echo Copying files...
	@[ -d binary ] || mkdir binary
	@rm -rf binary/*
	@cp build/$(TARGET).ds.gba build/$(TARGET).nds binary
	@mkdir binary/SlashEM
	@mkdir binary/SlashEM/save
	@touch binary/SlashEM/record
	@cp dat/nhshare dat/nhushare dat/license doc/Guidebook.txt doc/nethack.txt binary/SlashEM
	@cp sys/nds/NetHack/* binary/SlashEM
	@echo Done!  Copy everything in the 'binary' directory to your CF card.
	@echo Note: If you compiled without DLB defined, you must copy the files listed in
	@echo 'dat/dlb.lst to binary/SlashEM.  Try: cd dat; cp `cat dlb.lst` ../binary/SlashEM'
	@echo 

install: binary/$(TARGET).ds.gba
	cp binary/$(TARGET).nds /media/flash/default.nds
	umount /media/flash

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
