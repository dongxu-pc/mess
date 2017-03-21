#!/bin/bash
apt-get install ttf-wqy-microhei
cp *.ttf /usr/local/share/fonts/
chmod 644 /usr/local/share/fonts/*
mkfontscale
mkfontdir
fc-cache -fv
