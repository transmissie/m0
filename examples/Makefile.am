

M0_OPTIONS=  -T
M0= $(builddir)/../src/m0

all: parser.texi parser.m0 parser1.texi parser1.m0 parser2.texi parser2.m0 parser3.texi parser3.m0 preparser.texi preparser.m0 

parser.texi: $(srcdir)/doc.m0 $(srcdir)/parser.src | $(M0)
	$(M0) $^ -o $@

parser1.texi: $(srcdir)/doc.m0 $(srcdir)/parser1.src | $(M0)
	$(M0) $^ -o $@

parser2.texi: $(srcdir)/doc.m0 $(srcdir)/parser2.src | $(M0)
	$(M0) $^ -o $@

parser3.texi: $(srcdir)/doc.m0 $(srcdir)/parser3.src | $(M0)
	$(M0) $^ -o $@

preparser.texi: $(srcdir)/doc.m0 $(srcdir)/preparser.src | $(M0)
	$(M0) $^ -o $@

parser.m0: $(srcdir)/example.m0 $(srcdir)/parser.src | $(M0)
	$(M0) $(M0_OPTIONS) -t $@.trace $^ -o $@ > $@.log

parser1.m0: $(srcdir)/example.m0 $(srcdir)/parser1.src | $(M0)
	$(M0) $(M0_OPTIONS) -t $@.trace $^ -o $@ > $@.log

parser2.m0: $(srcdir)/example.m0 $(srcdir)/parser2.src | $(M0)
	$(M0) $(M0_OPTIONS) -t $@.trace $^ -o $@ > $@.log

parser3.m0: $(srcdir)/example.m0 $(srcdir)/parser3.src | $(M0)
	$(M0) $(M0_OPTIONS) -t $@.trace $^ -o $@ > $@.log

preparser.m0: $(srcdir)/example.m0 $(srcdir)/preparser.src | $(M0)
	$(M0) $(M0_OPTIONS) -t $@.trace $^ -o $@ > $@.log
