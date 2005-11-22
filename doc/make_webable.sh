#!/bin/sh

# this converts the UTF-8 string in the Content-Type meta tag in all
# html files to lower case, otherwise some browsers
# (e.g. Mozilla Firefox) have problems to detect the right encoding
# if it is put through a webserver

sed -i 's/charset=UTF-8/charset=utf-8/g' html/*.html
