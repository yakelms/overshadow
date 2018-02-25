app = overshadow
$(app):main.c
	gcc -g main.c -o $(app)

clean:
	rm -rf $(app) crypt_*

clr:
	rm -rf crypt_*
