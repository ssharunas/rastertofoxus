# ESC/POS CUPS Raster Driver - rastertofoxus

## Introduction

Driver for printing receipts with brandless Chinese POS printers.

## Instalation

The CUPS image development headers are required before compilation. In Ubuntu, these can be installed with:

    sudo apt-get install libcupsimage2-dev

The easiest way to install from source is to run the following from the base directory:

    make
    sudo make install

This will install the filter and PPD files in the standard CUPS filter and PPD directories
and show them in the CUPS printer selection screens.


## Authors

rastertofoxus is based on the rastertoescpos driver written by Chunlin Yao  
rastertoescpos is based on the rastertotpcl driver written by Sam Lown  
rastertotpcl is based on the rastertotec driver written by Patick Kong (SKE s.a.r.l).  
rastertotec is based on the rastertolabel driver included with the CUPS printing system by Easy Software Products.  

rastertofoxus is written by Šarūnas Gliebus
