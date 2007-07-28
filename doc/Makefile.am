configdir = $(sysconfdir)/splashy

man_MANS = splashy.1 splashy_config.1 splashy_update.1 splashy-config.xml.5 splashy-theme.xml.5
man_PODS = splashy.pod splashy_config.pod splashy_update.pod splashy-config.xml.pod splashy-theme.xml.pod
config_DATA = config.xml
noinst_DATA = theme.xml

clean-local:
	rm -f $(config_DATA) $(noinst_DATA) $(man_MANS)

%.1: %.pod 
	pod2man --section 1 --center "Splashy POSIX boot splash system" --date `date +%Y-%m-%d` $< > $@ 

%.5: %.pod 
	pod2man --section 5 --center "Splashy POSIX boot splash system" --date `date +%Y-%m-%d` $< > $@ 

EXTRA_DIST = \
	$(man_PODS) \
	config.xml.in theme.xml.in 

