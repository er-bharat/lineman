# Introduction
  
  This is a Line of code calculator it calculates the no of **comments** lines, **empty** lines, and 
  **actual** line of codes inside the source file.  
  Made this because more mature older ones like cloc etc have become very complex to use
  this one takes only 2 things a source ***file*** or a ***folder***  
  it does nothing else.  

## Look

``` 
 ➥ ./lineman /home/bharat/Documents/martian/Win8/experiment/Vlog 

FILES BY TYPE:
  qml      2
  ini      1
  qrc      1
  desktop  1
  cpp      1
  txt      1
  json     1
  svg      1

Main.qml ............................... e-    31  cs-    10  co-   137
HudOverlay.qml ------------------------- e-    31  cs-     9  co-   257
datas.ini .............................. e-     1  cs-     0  co-     7
resources.qrc -------------------------- e-     0  cs-     0  co-     6
Martian vlog.desktop ................... e-     1  cs-     0  co-    12
main.cpp ------------------------------- e-    39  cs-     6  co-   176
CMakeLists.txt ......................... e-     9  cs-     0  co-    35
compile_commands.json ------------------ e-     0  cs-     0  co-    20

icons/drawing.svg ...................... e-     1  cs-     1  co-   392

----------------------------------------------------------------------------
TOTAL ---------------------------------- e-   113  cs-    26  co-  1042


```
## features

- has one general c c++ style comments it is applied on all files if in code not specially supported
- 2nd extension based comment setup inside c source of lineman for those extensions changes its comment detection either single line or a multiline both are supported.

  
## build

  `gcc lineman.c -o lineman`  
  
## Install

- use distro package if provides one 
- or move the lineman to `/usr/bin/`

