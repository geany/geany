
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && apt-get install -y git

#######################################################################

RUN apt-get update
RUN apt-get install -y intltool
RUN apt-get install -y g++
RUN apt-get update
RUN apt-get install -y python3-docutils
RUN apt-get install -y python3
RUN apt-get install -y dbus  pkgconf
RUN apt-get install -y libopengl-dev
RUN apt-get install -y systemd-dev
RUN apt-get install -y systemd
RUN apt-get install -y mesa-vulkan-drivers libxrender-dev libxrandr-dev
RUN apt-get install -y libglx-dev
RUN apt-get install -y python3-packaging
RUN apt-get install -y libmount-dev
RUN apt-get install -y libenchant-2-dev
RUN apt-get install -y libgit2-dev
RUN apt-get install -y x11-common libglvnd-dev
RUN apt-get install -y dconf-service
RUN apt-get install -y humanity-icon-theme
RUN apt-get install -y libgtk-3-dev
RUN apt-get install -y libvte-2.91-dev
RUN apt-get install -y gnupg
RUN apt-get install -y libfakeroot
RUN apt-get install -y build-essential
RUN apt-get install -y autopoint
RUN apt-get install -y libtool

RUN apt-get update && apt-get install -y libegl-mesa0


RUN apt-get install -y yaru-theme-gtk

RUN apt-get update && apt-get install -y xdg-desktop-portal-gtk
# RUN apt-get install libcanberra-gtk-module libcanberra-gtk3-module
RUN gsettings set org.gnome.desktop.interface color-scheme prefer-dark

# RUN rm -rf /var/lib/apt/lists/*

RUN mkdir /home/geany
WORKDIR /home/geany
COPY .  /home/geany/
RUN ./autogen.sh
RUN make -j
RUN make install


WORKDIR /home
RUN git clone https://github.com/geany/geany-plugins.git
WORKDIR /home/geany-plugins
RUN sed -i 's/"Tree Browser"/"Folders"/g' treebrowser/src/treebrowser.c
RUN ./autogen.sh --enable-scope --enable-spellcheck --enable-gitchangebar
RUN make -j
RUN make install

CMD  printf 'usage\n podman run --net=host --env="DISPLAY" geany:latest\n' \
     && "geany"
