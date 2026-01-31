# Introduction
  
  This is a Line of code calculator it calculates the no of **comments** lines, **empty** lines, and 
  **actual** line of codes inside the source file.  
  Made this because more mature older ones like cloc etc have become very complex to use
  this one takes only 2 things a source ***file*** or a ***folder***  
  it does nothing else.  

## Look

``` 
➥ ./lineman /home/bharat/Documents/martian/Win8/win/list-windows   
CMakeLists.txt | e-10 cs-21 co-26  
wlr-foreign-toplevel-management-unstable-v1-client-protocol.h | e-37 cs-359 co-219  
wlr-foreign-toplevel-management-unstable-v1-protocol.c | e-13 cs-25 co-65  
wlr-foreign-toplevel-management-unstable-v1.xml | e-42 cs-2 co-226  
main.cpp | e-90 cs-62 co-393  
  
FILES BY TYPE:  
txt    1  
h      1  
c      1  
xml    1  
cpp    1  
  
TOTAL:   
e-192 cs-469 co-929 
```
## features

- has one general c c++ style comments it is applied on all files if in code not specially supported
- 2nd extension based comment setup inside c source of lineman for those extensions changes its comment detection either single line or a multiline both are supported.

  
## build

  `gcc lineman.c -o lineman`  
  
## Install

- use distro package if provides one 
- or move the lineman to `/usr/bin/`

