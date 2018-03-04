app = overshadow
RELEASE=1
ifeq ($(RELEASE),1)
	CFLAGS += -O3
endif
$(app):main.c
	gcc -g $(CFLAGS) main.c -o $(app)

clean:
	rm -rf $(app) crypt_*

clr:
	rm -rf crypt_*
