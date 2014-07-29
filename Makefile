.PHONY: all clean lnx64a lnx

all: lnx lnx64a

lnx64a:
	$(MAKE) -C src lnx64a

lnx:
	$(MAKE) -C src lnx

clean:
	$(MAKE) -C src clean
