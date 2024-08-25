
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && apt-get install -y git

#######################################################################

RUN apt-get update && apt-get install -y \
     intltool g++  python3-docutils python3 \
     dbus  pkgconf  libopengl-dev  systemd-dev  systemd \
     mesa-vulkan-drivers libxrender-dev libxrandr-dev  libglx-dev \
     python3-packaging  libmount-dev libenchant-2-dev libgit2-dev \
     x11-common libglvnd-dev dconf-service  humanity-icon-theme \
     libgtk-3-dev  libvte-2.91-dev  gnupg  libfakeroot \
     build-essential autopoint libtool

RUN apt-get update && apt-get install -y libegl-mesa0


RUN apt-get install -y yaru-theme-gtk  xdg-desktop-portal-gtk

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
