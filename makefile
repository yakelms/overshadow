app = overshadow
RELEASE=1
ifeq ($(RELEASE),1)
	CFLAGS += -O3
else
	CFLAGS += -g2 -O0
endif
$(app):main.c
	gcc $(CFLAGS) main.c -o $(app)

clean:
	rm -rf $(app) crypt_*

clr:
	rm -rf crypt_*
