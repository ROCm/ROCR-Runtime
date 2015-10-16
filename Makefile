.PHONY: all clean lnx64a lnx

all: lnx64a

deb:
	$(MAKE) -C src deb

lnx64a:
	$(MAKE) -C src lnx64a

lnx:
	$(MAKE) -C src lnx

clean:
	$(MAKE) -C src clean
