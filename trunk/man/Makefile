MANLINKS=isp_fini.3 isp_unit_create.3 isp_unit_destroy.3 \
	 isp_unit_write.3 isp_file_sink.3 isp_unit_fini.3 \
	 isp_meta_sink.3 isp_meta_set.3 isp_errx.3

all: $(MANLINKS)

isp_fini.3: 
	ln -s isp_init.3 $@
isp_unit_create.3:
	ln -s isp_unit_copy.3 $@
isp_unit_destroy.3:
	ln -s isp_unit_copy.3 $@
isp_unit_fini.3:
	ln -s isp_unit_init.3 $@
isp_unit_write.3:
	ln -s isp_unit_read.3 $@
isp_file_sink.3:
	ln -s isp_file_source.3 $@
isp_meta_sink.3:
	ln -s isp_meta_source.3 $@
isp_meta_set.3:
	ln -s isp_meta_get.3 $@
isp_errx.3:
	ln -s isp_err.3 $@

clean:
	rm -f *.ps $(MANLINKS)
