# overshadow

## Description
cast a shadow on your files
usage: `overshadow -e [-n workers_nb] [-m p[rocesses]|t[hreades] -i [/path/to/]plain_file [-o [/path/to/]shadow.file]`

wipe off the shadow from your files
usage: `overshadown -d [-n workers_nb] [-m p[rocesses]|t[hreades] -i [/path/to/]shadow.file -o [/path/to/]plain.file`


## Download and Install
### Install from source
```
git clone https://github.com/yakelms/overshadow.git ~/overshadow &&
make -C ~/overshadow &&
sudo make install -C ~/overshdow
```

### Run in docker
```
docker run -it --name overshadow -h docker yakel/overshadow
```


## Testing
### Function testing
```
make test -C ~/overshadow
```

### Performance testing
```
make performance -C ~/overshadow
```
