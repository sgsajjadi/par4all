#
# $RCSfile: config.makefile,v $ for hpfc scripts
#

SCRIPTS= 	hpfc \
		hpfc_directives \
		hpfc_add_warning \
		hpfc_generate_h \
		hpfc_compile \
		hpfc_generate_init \
		hpfc_delete \
		hpfc_install \
		hpfc_llcmd

FILES=
SFILES=		hpfc_interactive.c
RFILES=		hpfc_interactive

#
# Some rules

hpfc_interactive: hpfc_interactive.o
	$(RM) hpfc_interactive
	$(LD) $(LDFLAGS) \
		-o hpfc_interactive hpfc_interactive.o -lreadline -ltermcap
	chmod a-w hpfc_interactive

hpfc_interactive.o: hpfc_interactive.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c hpfc_interactive.c

clean:
	$(RM) hpfc_interactive.o hpfc_interactive *~

web: hpfc_directives
	cp hpfc_directives $(HOME)/public_html/
	chmod a+r $(HOME)/public_html/hpfc_directives

# that is all
#
