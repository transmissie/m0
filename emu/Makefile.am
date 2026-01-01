

# EXTRA_DIST = m4emu.texi m0start.texi m4v7.m0 m4posix.m0 m4bsd.m0 m4gnu.m0 m0start.m0  m4.src m0start.src docs.m0 emus.m0

M0_OPTIONS=  -T
M0= $(builddir)/../src/m0

all: m4emu.texi m0start.texi m4v7.m0 m4posix.m0 m4bsd.m0 m4gnu.m0 m0start.m0 


m4emu.texi: $(srcdir)/docs.m0 $(srcdir)/m4.src | $(M0)
	$(M0) $^ -o $@

m0start.texi: $(srcdir)/docs.m0 $(srcdir)/m0start.src | $(M0)
	$(M0) $^ -o $@

#cp m4emu.texi $(srcdir)/../doc

m4v7.m0: $(srcdir)/emus.m0 $(srcdir)/m4.src | $(M0)
	$(M0) $(M0_OPTIONS) -t $@.trace --define=v7=v7 --define=all $^ -o $@ > $@.log

m4posix.m0: $(srcdir)/emus.m0 $(srcdir)/m4.src | $(M0)
	$(M0) $(M0_OPTIONS) -t $@.trace --define=posix=posix --define=all $^ -o $@ > $@.log

m4bsd.m0: $(srcdir)/emus.m0 $(srcdir)/m4.src | $(M0)
	$(M0) $(M0_OPTIONS) -t $@.trace --define=bsd=bsd --define=all $^ -o $@ > $@.log

m4gnu.m0: $(srcdir)/emus.m0 $(srcdir)/m4.src | $(M0)
	$(M0) $(M0_OPTIONS) -t $@.trace --define=gnu=gnu --define=all $^ -o $@ > $@.log

m0start.m0: $(srcdir)/emus.m0 $(srcdir)/m0start.src | $(M0)
	$(M0) $(M0_OPTIONS) -t $@.trace --define=all $^ -o $@ > $@.log
