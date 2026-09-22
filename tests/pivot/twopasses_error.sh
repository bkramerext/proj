#!/bin/sh
cat colors.xml | proj ...:pivot[name,value,true]
