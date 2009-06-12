# $Id$

# The option used by default for validating. Can be overridden by the
# command line "make VOPT=..." or the environment variable:
VOPT	= -v --archive --diff

FIND	= find . -name '.svn' -type d -prune -o

.PHONY: clean
clean:
	$(MAKE) TARGET=. clean-target

# subdirectories to consider
TARGET	:= $(shell grep '^[a-zA-Z]' defaults)

.PHONY: clean-target
clean-target:
	for d in $(TARGET) ; do \
	  echo "### cleaning $$d" ; \
	  $(FIND) -name '*~' -type f -print0 \
	     -o -name 'core' -type f -print0 \
	     -o -name 'a.out' -type f -print0 \
	     -o -name 'out' -type f -print0 \
	     -o -name '*.filtered' -type f -print0 \
	     -o -name '*.o' -type f -print0 | xargs -0 $(RM) ; \
	  $(FIND) -name '*.database' -type d -print0 \
	     -o -name 'validation_results.*' -type d -print0 | \
		xargs -0 $(RM) -r ; \
	done
	$(RM) properties.rc a.out core *.o
	$(RM) -r RESULTS

.PHONY: validate
validate: clean-target
	PIPS_MORE=cat pips_validate $(VOPT) -V $(PWD) -O RESULTS $(TARGET)

.PHONY: accept
accept:
	manual_accept $(TARGET)

# extract private (restricted access) validation
.PHONY: private
private:
	if [ -d private/. ] ; then \
	  if [ -d private/.svn ] ; then \
	    svn up private/ ; \
	  else \
	    echo "ERROR: cannot update private" >&2 ; \
	  fi ; \
	else \
	  echo "checkout the private validation somewhere:"; \
	  echo "> svn co http://svnpriv.cri.ensmp.fr/svn/pipspriv/trunk ???"; \
	  echo "and link it here as 'private':"; \
	  echo "> ln -s ??? private"; \
	  echo "CAUTION: it MUST NOT be distributed..." ; \
	fi

# validate one sub directory
validate-%: %
	test -d $< && $(MAKE) TARGET=$< validate

# special handling of private
PRIV	= $(wildcard private/*)
PRIV.d	= $(shell for d in $(PRIV) ; do test -d $$d && echo $$d ; done)
validate-private:
	$(MAKE) TARGET="$(PRIV.d)" validate

# validate all subdirectories
ALL	= $(wildcard * private/*)
ALL.d	= $(shell for d in $(ALL) ; do test -d $$d && echo $$d ; done)
validate-all:
	$(MAKE) TARGET="$(ALL.d)" validate
