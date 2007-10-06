SVNURL        := https://isp.svn.sourceforge.net/svnroot/isp
VERSION       := $(shell awk '/[Vv]ersion:/ {print $$2}' META)
RELEASE       := $(shell awk '/[Rr]elease:/ {print $$2}' META)
TRUNKURL      := $(SVNURL)/trunk/isp
TAGURL        := $(SVNURL)/tags/isp-$(VERSION)


SUBDIRS		:= isp utils man doc htdocs

all clean:
	for subdir in $(SUBDIRS); do make -C $$subdir $@; done

check-vars:
	@echo "Release:  isp-$(VERSION)"
	@echo "Trunk:    $(TRUNKURL)"
	@echo "Tag:      $(TAGURL)"

rpms-working:
	make clean
	@scripts/build --snapshot $(BUILDFLAGS) .

rpms-trunk: check-vars
	@scripts/build --snapshot $(BUILDFLAGS) $(TRUNKURL)

rpms-release: check-vars
	@scripts/build --nosnapshot $(BUILDFLAGS) $(TAGURL)

tagrel:
	svn copy $(TRUNKURL) $(TAGURL)

